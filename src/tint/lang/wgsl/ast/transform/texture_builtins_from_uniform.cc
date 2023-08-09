// Copyright 2023 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tint/lang/wgsl/ast/transform/texture_builtins_from_uniform.h"

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/call.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/statement.h"
#include "src/tint/lang/wgsl/sem/variable.h"

#include "src/tint/utils/containers/hashmap.h"
#include "src/tint/utils/containers/vector.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform);
TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform::Config);
TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform::Result);

namespace tint::ast::transform {

namespace {

/// The member name of the texture builtin values array.
const char* kTextureBuiltinValuesMemberName = "texture_builtin_values";

bool ShouldRun(const Program* program) {
    for (auto* fn : program->AST().Functions()) {
        if (auto* sem_fn = program->Sem().Get(fn)) {
            for (auto* builtin : sem_fn->DirectlyCalledBuiltins()) {
                // OpenGLES GLSL version has no native support for the counterpart of
                // textureNumLevels (textureQueryLevels) and textureNumSamples (textureSamples)
                if (builtin->Type() == core::Function::kTextureNumLevels) {
                    return true;
                }
                if (builtin->Type() == core::Function::kTextureNumSamples) {
                    return true;
                }
            }
        }
    }
    return false;
}

}  // namespace

TextureBuiltinsFromUniform::TextureBuiltinsFromUniform() = default;
TextureBuiltinsFromUniform::~TextureBuiltinsFromUniform() = default;

/// PIMPL state for the transform
struct TextureBuiltinsFromUniform::State {
    /// Constructor
    /// @param program the source program
    /// @param in the input transform data
    /// @param out the output transform data
    explicit State(const Program* program, const DataMap& in, DataMap& out)
        : src(program), inputs(in), outputs(out) {}

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        auto* cfg = inputs.Get<Config>();
        if (cfg == nullptr) {
            b.Diagnostics().add_error(
                diag::System::Transform,
                "missing transform data for " +
                    std::string(tint::TypeInfo::Of<TextureBuiltinsFromUniform>().name));
            return resolver::Resolve(b);
        }

        if (!ShouldRun(ctx.src)) {
            return SkipTransform;
        }

        struct FunctionExtraParamInfo {
            // The extra passed in param that corresponds to the texture param.
            const ast::Parameter* param = nullptr;
            // id of this extra param
            // e.g. f(t0, t1, e0, e1)
            // e0 and e1 are extra params, their extra_idx are 0 and 1.
            size_t idx = 0;
        };

        // Store a map from function to a collection of extra param that needs adding.
        // e.g.
        // fn f(t: texture_2d<f32>) -> u32 {
        //   return textureNumLevels(t);
        // }
        // ->
        // fn f(t : texture_2d<f32>, tint_symbol : u32) -> u32 {
        //   return tint_symbol;
        // }
        Hashmap<const sem::Function*, Hashmap<const ast::Parameter*, FunctionExtraParamInfo, 8>, 8>
            fn_to_data;

        // For each callsite of the above functions, record a vector of extra call args that needs
        // inserting. e.g. f(tex)
        // ->
        // f(tex, internal_uniform.texture_builtin_values[0u][0u]), if tex is from a global
        // variable, storing the BindingPoint. or f(tex, extra_param_tex), if tex is from a function
        // param, storing the texture function parameter pointer.
        Hashmap<const sem::Call*, Vector<std::variant<BindingPoint, const ast::Parameter*>, 8>, 8>
            call_to_data;

        // A map from global variable texture as arg of builtin calls to the function parameter it
        // meant to be replaced with. e.g. textureNumLevels(tex) ->
        // internal_uniform.texture_builtin_values[0u][0u]
        Hashmap<const sem::Variable*, const ast::Parameter*, 8> var_to_replace;

        // Iterate each texture builtin calls, populate fn_to_data, call_to_data, var_to_replace.
        IterateInterestedTextureBuiltinsCall([&](const sem::Function* original_parent_fn_sem,
                                                 const CallExpression* call_expr,
                                                 core::Function builtin_function_type,
                                                 const sem::Variable* texture_sem) {
            // Find all global texture variables that ever passed as the arg of current builtin
            // function.
            Vector<BindingPoint, 8> bindings;

            std::queue<std::pair<const sem::Variable*, const sem::Function*>> texture_sems_queue;
            texture_sems_queue.push({texture_sem, original_parent_fn_sem});

            while (!texture_sems_queue.empty()) {
                auto* cur_texture_sem = texture_sems_queue.front().first;
                auto* parent_fn_sem = texture_sems_queue.front().second;
                texture_sems_queue.pop();

                auto* global_var = tint::As<sem::GlobalVariable>(cur_texture_sem);

                if (global_var) {
                    // This texture variable is a global variable.
                    // Record the binding.
                    bindings.Push(global_var->BindingPoint().value());
                } else {
                    // This texture variable is a function parameter.
                    // Need to backtrack to the callsite where the global texture variable is passed
                    // in to record the binding.
                    auto* var = tint::As<sem::Variable>(cur_texture_sem);
                    auto& param_to_info = fn_to_data.GetOrCreate(parent_fn_sem, [&] {
                        return Hashmap<const ast::Parameter*, FunctionExtraParamInfo, 8>();
                    });

                    // Find which function param is the current one
                    size_t texture_param_id;
                    const ast::Parameter* new_param = nullptr;
                    for (size_t i = 0; i < parent_fn_sem->Declaration()->params.Length(); i++) {
                        auto* p = parent_fn_sem->Declaration()->params[i];
                        if (p->As<Variable>() == var->Declaration()) {
                            // Record the idx of the param to find the args[idx] of callsites later.
                            texture_param_id = i;

                            // Record a new u32 param to this function to add in a later ctx.Replace
                            // step.
                            new_param = b.Param(b.Sym(), b.ty.u32());
                            param_to_info.Add(
                                p, FunctionExtraParamInfo{new_param, param_to_info.Count()});

                            var_to_replace.Add(var, new_param);

                            break;
                        }
                    }
                    TINT_ASSERT(new_param != nullptr);

                    // For each callsite, if the passed in argument is a global variable
                    // Get the binding point, add it to bindingpoint_to_data.
                    // If not, i.e. still a function param, keep backtracking until finding the
                    // global variable texture passed in.
                    for (auto* parent_fn_call : parent_fn_sem->CallSites()) {
                        // The value expression can either be a global texture variable, or function
                        // parameter.
                        auto* t = parent_fn_call->Arguments()[texture_param_id]->RootIdentifier();
                        TINT_ASSERT(t != nullptr);
                        // Push to queue and keep backtracking
                        texture_sems_queue.push({t, parent_fn_call->Stmt()->Function()});

                        auto& args = call_to_data.GetOrCreate(parent_fn_call, [&] {
                            return Vector<std::variant<BindingPoint, const ast::Parameter*>, 8>();
                        });

                        auto* t_global_var = tint::As<sem::GlobalVariable>(t);
                        if (t_global_var) {
                            args.Push(t_global_var->BindingPoint().value());
                        } else {
                            const ast::Parameter* tex_param = nullptr;
                            for (auto* param :
                                 parent_fn_call->Stmt()->Function()->Declaration()->params) {
                                if (param->As<Variable>() == t->Declaration()) {
                                    tex_param = param;
                                    break;
                                }
                            }
                            TINT_ASSERT(tex_param != nullptr);

                            args.Push(tex_param);
                        }
                    }
                }
            }

            // Record any visited binding to result.
            for (const BindingPoint& binding : bindings) {
                auto iter = bindpoint_to_data.find(binding);
                if (iter == bindpoint_to_data.end()) {
                    TextureBuiltinsFromUniformOptions::Field dataType;
                    switch (builtin_function_type) {
                        case core::Function::kTextureNumLevels:
                            dataType = TextureBuiltinsFromUniformOptions::Field::TextureNumLevels;
                            break;
                        case core::Function::kTextureNumSamples:
                            dataType = TextureBuiltinsFromUniformOptions::Field::TextureNumSamples;
                            break;
                        default:
                            TINT_UNREACHABLE()
                                << "unsupported builtin function type " << builtin_function_type;
                    }

                    // For one texture type, it could only be passed into one of the
                    // textureNumLevels or textureNumSamples because their accepting param texture
                    // type is different. There cannot be a binding entry with both dataType.
                    uint32_t index = bindpoint_to_data.size();
                    bindpoint_to_data.emplace(
                        binding, Result::DataEntry{
                                     dataType, index * static_cast<uint32_t>(sizeof(uint32_t))});
                }
            }
        });

        if (bindpoint_to_data.empty()) {
            return SkipTransform;
        }

        // If there's a function data recorded, we need to insert some extra params and args.
        if (!fn_to_data.IsEmpty()) {
            for (auto pair : fn_to_data) {
                auto* fn = pair.key;

                // Reorder the param to a vector to make sure params are in the correct order.
                std::vector<const ast::Parameter*> extra_params_in_order(pair.value.Count());
                for (auto t_p : pair.value) {
                    extra_params_in_order[t_p.value.idx] = t_p.value.param;
                }

                for (auto p : extra_params_in_order) {
                    ctx.InsertBack(fn->Declaration()->params, p);
                }
            }

            for (auto pair : call_to_data) {
                auto* call = pair.key;
                for (auto new_arg_info : pair.value) {
                    if (std::holds_alternative<BindingPoint>(new_arg_info)) {
                        // This texture is a global variable with binding point.
                        // Read builtin value from uniform buffer.
                        auto* builtin_value = GetUniformValue(std::get<BindingPoint>(new_arg_info));
                        ctx.InsertBack(call->Declaration()->args, builtin_value);
                    } else {
                        // Otherwise this value comes from a function param
                        auto* parent_fn_sem = call->Stmt()->Function();
                        auto* param = fn_to_data.Get(parent_fn_sem)
                                          ->Get(std::get<const ast::Parameter*>(new_arg_info))
                                          ->param;
                        ctx.InsertBack(call->Declaration()->args, b.Expr(param->As<Variable>()));
                    }
                }
            }
        }

        // Replace any texture builtin calls with retreiving values from uniform buffer directly, or
        // with a function param.
        IterateInterestedTextureBuiltinsCall(
            [&](const sem::Function* parent_fn_sem, const CallExpression* call_expr,
                core::Function builtin_function_type, const sem::Variable* texture_sem) {
                auto* global_var = tint::As<sem::GlobalVariable>(texture_sem);
                if (!global_var) {
                    // The texture is a function parameter.

                    auto* var = tint::As<sem::Variable>(texture_sem);
                    auto replace_param = var_to_replace.Get(var);
                    TINT_ASSERT(replace_param.has_value());
                    ctx.Replace(call_expr, b.Expr(replace_param.value()->As<Variable>()));

                    return;
                }

                // The texture is a global variable.
                auto binding = global_var->BindingPoint();
                TINT_ASSERT(binding);
                ctx.Replace(call_expr, GetUniformValue(*binding));
            });

        outputs.Add<Result>(bindpoint_to_data);

        ctx.Clone();
        return resolver::Resolve(b);
    }

  private:
    /// The source program
    const Program* const src;
    /// The transform inputs
    const DataMap& inputs;
    /// The transform outputs
    DataMap& outputs;
    /// The target program builder
    ProgramBuilder b;
    /// The clone context
    program::CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};

    /// The bindpoint to byte offset and field to pass out in transform result.
    /// Note: because this transform must be run before CombineSampler and BindingRemapper,
    /// the binding number here is before remapped.
    Result::BindingPointDataInfo bindpoint_to_data;

    /// The internal uniform buffer
    const Variable* ubo = nullptr;
    /// Get or create a ubo including an array containing texture builtin values.
    /// @returns the symbol of the uniform buffer variable.
    Symbol GetUboSym() {
        if (!ubo) {
            auto* cfg = inputs.Get<Config>();
            // Emit an array<vec4<u32>, N>, where N is 1/4 number of elements.
            // We do this because UBOs require an element stride that is 16-byte
            // aligned.
            auto* new_member = b.Member(
                kTextureBuiltinValuesMemberName,
                b.ty.array(b.ty.vec4(b.ty.u32()), u32((bindpoint_to_data.size() / 4) + 1)));

            // Find if there's any existing glboal variable using the same cfg->ubo_binding
            for (auto* var : src->AST().Globals<Var>()) {
                if (var->HasBindingPoint()) {
                    auto* global_sem = src->Sem().Get<sem::GlobalVariable>(var);

                    // The original binding point
                    BindingPoint binding_point = *global_sem->BindingPoint();

                    if (binding_point == cfg->ubo_binding) {
                        // This ubo_binding struct already exists.
                        // which should only be added by other *FromUniform transforms.
                        // Replace it with a new struct including the new_member.
                        // Then remove the old structure global declaration.

                        ubo = var->As<Variable>();

                        auto* ty = global_sem->Type()->UnwrapRef();
                        auto* str = ty->As<sem::Struct>();
                        if (TINT_UNLIKELY(!str)) {
                            TINT_ICE() << "existing ubo binding " << cfg->ubo_binding
                                       << " is not a struct.";
                            return ctx.Clone(ubo->name->symbol);
                        }

                        ctx.InsertBack(str->Declaration()->members, new_member);

                        return ctx.Clone(ubo->name->symbol);
                    }
                }
            }

            auto* buffer_size_struct = b.Structure(b.Sym(), tint::Vector{
                                                                new_member,
                                                            });
            ubo = b.GlobalVar(b.Sym(), b.ty.Of(buffer_size_struct), core::AddressSpace::kUniform,
                              b.Group(AInt(cfg->ubo_binding.group)),
                              b.Binding(AInt(cfg->ubo_binding.binding)));
        }
        return ubo->name->symbol;
    }

    /// Get the expression of retrieving the builtin value from the uniform buffer.
    /// @param binding of the global variable.
    /// @returns an expression of the builtin value.
    const ast::Expression* GetUniformValue(const BindingPoint& binding) {
        auto iter = bindpoint_to_data.find(binding);
        TINT_ASSERT(iter != bindpoint_to_data.end());

        // Convert byte offset to index.
        uint32_t index = iter->second.second / sizeof(uint32_t);

        // Load the builtin value from the UBO.
        uint32_t array_index = index / 4;
        auto* vec_expr = b.IndexAccessor(
            b.MemberAccessor(GetUboSym(), kTextureBuiltinValuesMemberName), u32(array_index));
        uint32_t vec_index = index % 4;

        auto* builtin_value = b.IndexAccessor(vec_expr, u32(vec_index));
        return builtin_value;
    }

    /// Iterate over all textureNumLevels/textureNumSamples builtins that operate on
    /// a texture variable.
    /// @param functor of type void(const sem::Function* original_parent_fn_sem, const
    /// CallExpression* call_expr, core::Function builtin_function_type, const sem::Variable*
    /// texture_sem)
    template <typename F>
    void IterateInterestedTextureBuiltinsCall(F&& functor) {
        auto& sem = src->Sem();

        // Find all calls to the arrayLength() builtin.
        for (auto* node : src->ASTNodes().Objects()) {
            auto* call_expr = node->As<CallExpression>();
            if (!call_expr) {
                continue;
            }

            auto* call = sem.Get(call_expr)->UnwrapMaterialize()->As<sem::Call>();
            auto* builtin = call->Target()->As<sem::Builtin>();
            if (!builtin) {
                continue;
            }

            if (builtin->Type() != core::Function::kTextureNumLevels &&
                builtin->Type() != core::Function::kTextureNumSamples) {
                continue;
            }

            if (auto* call_stmt = call->Stmt()->Declaration()->As<CallStatement>()) {
                if (call_stmt->expr == call_expr) {
                    // textureNumLevels() / textureNumSamples() is used as a statement.
                    // The argument expression must be side-effect free, so just drop the statement.
                    RemoveStatement(ctx, call_stmt);
                    continue;
                }
            }
            auto* texture_expr = call_expr->args[0];
            auto* texture_sem = sem.GetVal(texture_expr)->RootIdentifier();
            if (TINT_UNLIKELY(!texture_sem)) {
                TINT_ICE() << "expected arg is a texture";
                break;
            }
            functor(call->Stmt()->Function(), call_expr, builtin->Type(), texture_sem);
        }
    }
};

Transform::ApplyResult TextureBuiltinsFromUniform::Apply(const Program* src,
                                                         const DataMap& inputs,
                                                         DataMap& outputs) const {
    return State{src, inputs, outputs}.Run();
}

TextureBuiltinsFromUniform::Config::Config(BindingPoint ubo_bp) : ubo_binding(ubo_bp) {}
TextureBuiltinsFromUniform::Config::Config(const Config&) = default;
TextureBuiltinsFromUniform::Config& TextureBuiltinsFromUniform::Config::operator=(const Config&) =
    default;
TextureBuiltinsFromUniform::Config::~Config() = default;

TextureBuiltinsFromUniform::Result::Result(BindingPointDataInfo bindpoint_to_data_in)
    : bindpoint_to_data(std::move(bindpoint_to_data_in)) {}
TextureBuiltinsFromUniform::Result::Result(const Result&) = default;
TextureBuiltinsFromUniform::Result::~Result() = default;

}  // namespace tint::ast::transform

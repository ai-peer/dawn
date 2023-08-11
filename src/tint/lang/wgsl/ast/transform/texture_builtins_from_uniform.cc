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
#include "src/tint/lang/wgsl/sem/module.h"
#include "src/tint/lang/wgsl/sem/statement.h"
#include "src/tint/lang/wgsl/sem/variable.h"

#include "src/tint/utils/containers/hashmap.h"
#include "src/tint/utils/containers/vector.h"
#include "src/tint/utils/rtti/switch.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform);
TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform::Config);
TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform::Result);

namespace tint::ast::transform {

namespace {

/// The member name of the texture builtin values.
const char* kTextureBuiltinValuesMemberName = "texture_builtin_value";

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

        // The dependency order declartions guaranteed that we traverse interested functions in the
        // following order:
        // 1. texture builtins
        // 2. user function directly calls texture builtins
        // 3. user function calls 2.
        // 4. user function calls 3.
        // ...
        // n. entry point function.
        for (auto* fn_decl : sem.Module()->DependencyOrderedDeclarations()) {
            if (auto* fn = sem.Get<sem::Function>(fn_decl)) {
                for (auto* call : fn->DirectCalls()) {
                    auto* call_expr = call->Declaration();

                    tint::Switch(
                        call->Target(),
                        [&](const sem::Builtin* builtin) {
                            if (builtin->Type() != core::Function::kTextureNumLevels &&
                                builtin->Type() != core::Function::kTextureNumSamples) {
                                return;
                            }
                            if (auto* call_stmt =
                                    call->Stmt()->Declaration()->As<CallStatement>()) {
                                if (call_stmt->expr == call->Declaration()) {
                                    // textureNumLevels() / textureNumSamples() is used as a
                                    // statement. The argument expression must be side-effect free,
                                    // so just drop the statement.
                                    RemoveStatement(ctx, call_stmt);
                                    return;
                                }
                            }

                            auto* texture_expr = call->Declaration()->args[0];
                            auto* texture_sem = sem.GetVal(texture_expr)->RootIdentifier();
                            TINT_ASSERT(texture_sem);

                            TextureBuiltinsFromUniformOptions::Field dataType =
                                GetFieldFromBuiltinFunctionType(builtin->Type());

                            tint::Switch(
                                texture_sem,
                                [&](const sem::GlobalVariable* global) {
                                    // This texture variable is a global variable.
                                    auto binding = GetAndRecordGlobalBinding(global, dataType);
                                    // Record the call and binding to be replaced later.
                                    builtin_to_replace.Add(call_expr, binding);
                                },
                                [&](const sem::Variable* variable) {
                                    // This texture variable is a user function parameter.
                                    auto new_param =
                                        GetAndRecordFunctionParameter(fn, variable, dataType);
                                    // Record the call and new_param to be replaced later.
                                    builtin_to_replace.Add(call_expr, new_param);
                                },
                                [&](Default) {
                                    TINT_ICE() << "unexpected texture root identifier";
                                });
                        },
                        [&](const sem::Function* user_fn) {
                            auto user_param_to_info = fn_to_data.Find(user_fn);
                            if (!user_param_to_info) {
                                // Uninterested function not calling texture builtins with function
                                // texture param.
                                return;
                            }
                            for (size_t i = 0; i < call->Declaration()->args.Length(); i++) {
                                auto param = user_fn->Declaration()->params[i];
                                auto info = (*user_param_to_info).Get(param);
                                if (info.has_value()) {
                                    auto* arg = call->Arguments()[i];
                                    auto* texture_sem = arg->RootIdentifier();
                                    auto& args = call_to_data.GetOrCreate(call_expr, [&] {
                                        return Vector<
                                            std::variant<BindingPoint, const ast::Parameter*>, 4>();
                                    });

                                    tint::Switch(
                                        texture_sem,
                                        [&](const sem::GlobalVariable* global) {
                                            // This texture variable is a global variable.
                                            auto binding =
                                                GetAndRecordGlobalBinding(global, info->field);
                                            // Record the binding to add to args.
                                            args.Push(binding);
                                        },
                                        [&](const sem::Variable* variable) {
                                            // This texture variable is a user function parameter.
                                            auto new_param = GetAndRecordFunctionParameter(
                                                fn, variable, info->field);
                                            // Record adding extra function parameter
                                            args.Push(new_param);
                                        },
                                        [&](Default) {
                                            TINT_ICE() << "unexpected texture root identifier";
                                        });
                                }
                            }
                        });
                }
            }
        }

        // If there's no interested texture builtin at all, skip the transform.
        if (bindpoint_to_data.empty()) {
            return SkipTransform;
        }

        // If there's any function needs extra param, add them now.
        if (!fn_to_data.IsEmpty()) {
            for (auto pair : fn_to_data) {
                auto* fn = pair.key;

                // Reorder the param to a vector to make sure params are in the correct order.
                Vector<const ast::Parameter*, 4> extra_params_in_order;
                extra_params_in_order.Resize(pair.value.Count());
                for (auto t_p : pair.value) {
                    extra_params_in_order[t_p.value.extra_idx] = t_p.value.param;
                }

                for (auto p : extra_params_in_order) {
                    ctx.InsertBack(fn->Declaration()->params, p);
                }
            }
        }

        // Replace all interested texture builtin calls.
        for (auto pair : builtin_to_replace) {
            auto call = pair.key;
            // auto call_sem = pair.key;
            // auto call = call_sem->Declaration();
            // auto call =
            // sem.Get(call_sem->Declaration())->UnwrapMaterialize()->As<sem::Call>()->Declaration();
            if (std::holds_alternative<BindingPoint>(pair.value)) {
                // This texture is a global variable with binding point.
                // Read builtin value from uniform buffer.
                auto* builtin_value = GetUniformValue(std::get<BindingPoint>(pair.value));
                ctx.Replace(call, builtin_value);
            } else {
                // Otherwise this value comes from a function param
                auto* param = std::get<const ast::Parameter*>(pair.value);
                ctx.Replace(call, b.Expr(param));
            }
        }

        // Insert all extra args to interested function calls.
        for (auto pair : call_to_data) {
            auto call = pair.key;
            for (auto new_arg_info : pair.value) {
                if (std::holds_alternative<BindingPoint>(new_arg_info)) {
                    // This texture is a global variable with binding point.
                    // Read builtin value from uniform buffer.
                    auto* builtin_value = GetUniformValue(std::get<BindingPoint>(new_arg_info));
                    ctx.InsertBack(call->args, builtin_value);
                } else {
                    // Otherwise this value comes from a function param
                    auto* param = std::get<const ast::Parameter*>(new_arg_info);
                    ctx.InsertBack(call->args, b.Expr(param));
                }
            }
        }

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
    /// Alias to the semantic info in ctx.src
    const sem::Info& sem = ctx.src->Sem();

    /// The bindpoint to byte offset and field to pass out in transform result.
    /// For one texture type, it could only be passed into one of the
    /// textureNumLevels or textureNumSamples because their accepting param texture
    /// type is different. There cannot be a binding entry with both field type.
    /// Note: because this transform must be run before CombineSampler and BindingRemapper,
    /// the binding number here is before remapped.
    Result::BindingPointDataInfo bindpoint_to_data;

    struct FunctionExtraParamInfo {
        // The kind of texture information this parameter holds.
        TextureBuiltinsFromUniformOptions::Field field =
            TextureBuiltinsFromUniformOptions::Field::TextureNumLevels;

        // The extra passed in param that corresponds to the texture param.
        const ast::Parameter* param = nullptr;

        // id of this extra param e.g. f(t0, foo, t1, e0, e1) e0 and e1 are extra params, their
        // extra_idx are 0 and 1. This is to help sort extra ids in the correct order.
        size_t extra_idx = 0;
    };

    /// Store a map from function to a collection of extra param that needs adding.
    /// The value of the map is made a map instead of a vector to make look for the param easier
    /// for call sites. e.g. fn f(t: texture_2d<f32>) -> u32 {
    ///   return textureNumLevels(t);
    /// }
    /// ->
    /// fn f(t : texture_2d<f32>, tint_symbol : u32) -> u32 {
    ///   return tint_symbol;
    /// }
    Hashmap<const sem::Function*, Hashmap<const ast::Parameter*, FunctionExtraParamInfo, 4>, 8>
        fn_to_data;

    /// For each callsite of the above functions, record a vector of extra call args that needs
    /// inserting. e.g. f(tex)
    /// ->
    /// f(tex, internal_uniform.texture_builtin_values[0u][0u]), if tex is from a global
    /// variable, storing the BindingPoint. or f(tex, extra_param_tex), if tex is from a function
    /// param, storing the texture function parameter pointer.
    Hashmap<const CallExpression*, Vector<std::variant<BindingPoint, const ast::Parameter*>, 4>, 8>
        call_to_data;

    /// Texture builtin calls to be replaced by either uniform value or function parameter.
    Hashmap<const CallExpression*, std::variant<BindingPoint, const ast::Parameter*>, 8>
        builtin_to_replace;

    /// index: (byte offset / 4) from bindpoint_to_data for each builtin values
    /// Storing the u32 scalar symbol for each builtin values in the ubo struct.
    Vector<Symbol, 16> builtin_value_syms;

    /// The internal uniform buffer
    const Variable* ubo = nullptr;
    /// Get or create a ubo including u32 scalars for texture builtin values.
    /// @returns the symbol of the uniform buffer variable.
    Symbol GetUboSym() {
        if (!ubo) {
            auto* cfg = inputs.Get<Config>();

            builtin_value_syms.Resize(bindpoint_to_data.size());
            Vector<const ast::StructMember*, 16> new_members;
            new_members.Resize(bindpoint_to_data.size());
            for (auto it : bindpoint_to_data) {
                // Emit a u32 scalar for each binding that needs builtin value passed in.
                auto sym = b.Symbols().New(kTextureBuiltinValuesMemberName);
                size_t i = it.second.second / sizeof(uint32_t);
                builtin_value_syms[i] = sym;
                new_members[i] = b.Member(sym, b.ty.u32());
            }

            // Find if there's any existing glboal variable using the same cfg->ubo_binding
            for (auto* var : src->AST().Globals<Var>()) {
                if (var->HasBindingPoint()) {
                    auto* global_sem = sem.Get<sem::GlobalVariable>(var);

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

                        for (auto new_member : new_members) {
                            ctx.InsertBack(str->Declaration()->members, new_member);
                        }

                        return ctx.Clone(ubo->name->symbol);
                    }
                }
            }

            auto* buffer_size_struct = b.Structure(b.Sym(), tint::Vector{
                                                                new_members,
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
        auto* builtin_value = b.MemberAccessor(GetUboSym(), builtin_value_syms[index]);
        return builtin_value;
    }

    /// Get and return the binding of the global texture variable. Record in bindpoint_to_data if
    /// first visited.
    /// @param global global variable of the texture variable.
    /// @param field type of the interested builtin function data related to this texture.
    /// @returns binding of the global variable.
    BindingPoint GetAndRecordGlobalBinding(const sem::GlobalVariable* global,
                                           TextureBuiltinsFromUniformOptions::Field field) {
        auto binding = global->BindingPoint().value();
        auto iter = bindpoint_to_data.find(binding);
        if (iter == bindpoint_to_data.end()) {
            // First visit, recording the binding.
            uint32_t index = static_cast<uint32_t>(bindpoint_to_data.size());
            bindpoint_to_data.emplace(
                binding, Result::DataEntry{field, index * static_cast<uint32_t>(sizeof(uint32_t))});
        }
        return binding;
    }

    /// Find which function param is the given texture variable.
    /// Add a new u32 param relates to this texture param. Record in fn_to_data if first visited.
    /// @param fn the current function scope.
    /// @param var the texture variable.
    /// @param field type of the interested builtin function data related to this texture.
    /// @returns the new u32 function parameter.
    const ast::Parameter* GetAndRecordFunctionParameter(
        const sem::Function* fn,
        const sem::Variable* var,
        TextureBuiltinsFromUniformOptions::Field field) {
        auto& param_to_info = fn_to_data.GetOrCreate(
            fn, [&] { return Hashmap<const ast::Parameter*, FunctionExtraParamInfo, 4>(); });

        const ast::Parameter* param = nullptr;
        for (auto p : fn->Declaration()->params) {
            if (p->As<Variable>() == var->Declaration()) {
                param = p;
            }
        }
        TINT_ASSERT(param);
        // Get or record a new u32 param to this function if first visited.
        auto entry = param_to_info.Get(param);
        if (entry.has_value()) {
            return entry->param;
        }
        const ast::Parameter* new_param = b.Param(b.Sym(), b.ty.u32());
        param_to_info.Add(param, FunctionExtraParamInfo{field, new_param, param_to_info.Count()});
        return new_param;
    }

    /// Get the uniform options field for the builtin function.
    /// @param type of the builtin function
    /// @returns corresponding TextureBuiltinsFromUniformOptions::Field for the builtin
    static TextureBuiltinsFromUniformOptions::Field GetFieldFromBuiltinFunctionType(
        core::Function type) {
        switch (type) {
            case core::Function::kTextureNumLevels:
                return TextureBuiltinsFromUniformOptions::Field::TextureNumLevels;
            case core::Function::kTextureNumSamples:
                return TextureBuiltinsFromUniformOptions::Field::TextureNumSamples;
            default:
                TINT_UNREACHABLE() << "unsupported builtin function type " << type;
        }
        return TextureBuiltinsFromUniformOptions::Field::TextureNumLevels;
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

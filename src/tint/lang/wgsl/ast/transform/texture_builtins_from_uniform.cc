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

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "src/tint/lang/wgsl/ast/transform/simplify_pointers.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/lang/wgsl/sem/call.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/statement.h"
#include "src/tint/lang/wgsl/sem/variable.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform);
TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform::Config);
TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::TextureBuiltinsFromUniform::Result);

namespace tint::ast::transform {

namespace {

bool ShouldRun(const Program* program) {
    for (auto* fn : program->AST().Functions()) {
        if (auto* sem_fn = program->Sem().Get(fn)) {
            for (auto* builtin : sem_fn->DirectlyCalledBuiltins()) {
                // glsl no native support functions
                if (builtin->Type() == builtin::Function::kTextureNumLevels) {
                    return true;
                }
                if (builtin->Type() == builtin::Function::kTextureNumSamples) {
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
                    std::string(utils::TypeInfo::Of<TextureBuiltinsFromUniform>().name));
            return Program(std::move(b));
        }

        if (!ShouldRun(ctx.src)) {
            return SkipTransform;
        }

        const char* kTextureBuiltinValuesMemberName = "texture_builtin_values";

        // Note: because this transform must be run before CombineSampler and BindingRemapper,
        // the binding number here is before remapped.
        Result::BindingPointDataInfo bindpoint_to_data;

        // struct FunctionInfo {
        //     // function parameter texture variables that needs to pass in extra param for builtin
        //     values. std::vector<const sem::Variable*> texture_sems;
        // };

        // std::unordered_map<sem::Function*,
        //     std::vector<BindingPoint>> fn_to_extra_bindpoint;
        // std::unordered_map<const sem::Function*, std::vector<const ast::Parameter*>> fn_to_data;

        // For each function, record a list of texture param to extra passed in texture builtin
        // value param. std::unordered_map<const sem::Function*, std::unordered_map<const
        // ast::Parameter*, const ast::Parameter*>> fn_to_data;

        // How to preserve the order to be same as callsite?
        std::unordered_map<const sem::Function*,
                           std::map<const ast::Parameter*, const ast::Parameter*>>
            fn_to_data;
        // std::vector<const sem::Variable*>> fn_to_data;

        struct CallArgInfo {
            // If the arg needs to fetch from a builtin uniform buffer
            std::optional<BindingPoint> binding;
            // If the texture comes from a function parameter
            const ast::Parameter* tex_param = nullptr;
        };
        // For each callsite, record a vector of extra call args to insert.
        std::unordered_map<const sem::Call*, std::vector<CallArgInfo>> call_to_data;
        // std::unordered_map<const sem::Call*, std::vector<std::optional<BindingPoint>>>
        // call_to_data;

        // texture variable to passed in builtin value function parameter

        // global variable texture builtin call to replace with a function parameter
        std::unordered_map<const sem::Variable*, const ast::Parameter*>
            var_to_replace;  // replace param

        // std::unordered_map<const sem::Variable*, size_t>
        //     var_to_param_id;  // replace param

        // // Builtin call to function parameter replacement info
        // // if second is nullptr, meaning it should be replaced with direct unfirom buffer lookup
        // std::unordered_map<const CallExpression*, const sem::Parameter *>
        // builtin_to_replace_param;

        // // If there is texture builtin called in function
        // // needs to expand uniform buffer size to number of textures
        // // pass in uint32_t
        // auto& sem = ctx.src->Sem();
        // bool texture_builtin_calls_inside_function = false;

        // Iterate each texture builtin calls, record values of what textures that need to pass in.
        IterateInterestedTextureBuiltinsCall([&](const sem::Function* original_parent_fn_sem,
                                                 const CallExpression* call_expr,
                                                 builtin::Function builtin_function_type,
                                                 //  const sem::GlobalVariable* var) {
                                                 const sem::Variable* texture_sem) {
            // Find all global texture variables that ever passed as the arg of current builtin
            // function.
            std::vector<BindingPoint> bindings;

            // std::queue<const sem::Variable*> texture_sems_queue;
            // texture_sems_queue.push(texture_sem);
            std::queue<std::pair<const sem::Variable*, const sem::Function*>> texture_sems_queue;
            texture_sems_queue.push({texture_sem, original_parent_fn_sem});

            // const sem::Function* child_fn_sem = nullptr;

            while (!texture_sems_queue.empty()) {
                // auto* cur_texture_sem = texture_sems_queue.front();
                auto* cur_texture_sem = texture_sems_queue.front().first;
                auto* parent_fn_sem = texture_sems_queue.front().second;
                texture_sems_queue.pop();

                auto* global_var = tint::As<sem::GlobalVariable>(cur_texture_sem);

                if (global_var) {
                    // Found the global texture variable essentially passed as the arg for the
                    // builtin_function_type Add to bindpoint_to_data And record if the data needs
                    // to be passed down to any custom function as a function parameter, to modify
                    // later.
                    bindings.push_back(global_var->BindingPoint().value());
                } else {
                    size_t parent_fn_params_length = parent_fn_sem->Declaration()->params.Length();
                    printf("\n !!!!: func: %s  %lu\n\n\n",
                           parent_fn_sem->Declaration()->name->symbol.Name().c_str(),
                           parent_fn_params_length);

                    // This texture variable is a function parameter.
                    // Backtrack to the callsite where the global texture variable is passed in.
                    auto* var = tint::As<sem::Variable>(cur_texture_sem);

                    // Record parent_fn_sem to append param and replace callsites later
                    auto iter = fn_to_data.find(parent_fn_sem);
                    if (iter == fn_to_data.end()) {
                        iter = fn_to_data.insert({parent_fn_sem, {}}).first;
                    }

                    // Find which fn param is the current one
                    // for (auto* p : parent_fn_sem->Declaration()->params)

                    size_t texture_param_id;
                    const ast::Parameter* new_param = nullptr;
                    for (size_t i = 0; i < parent_fn_sem->Declaration()->params.Length(); i++) {
                        auto* p = parent_fn_sem->Declaration()->params[i];
                        // auto* p_sem = sem.Get(p);
                        if (p->As<Variable>() == var->Declaration()) {
                            // if (p->As<Variable>()->name->symbol ==
                            // var->Declaration()->name->symbol) { TINT_ASSERT(Transform,
                            // p->type->Is<type::Texture>())
                            printf("\n !!!!: param: %s %s\n\n\n",
                                   p->As<Variable>()->name->symbol.Name().c_str(),
                                   var->Declaration()->name->symbol.Name().c_str());

                            // Record the idx of the param to find the args[idx] of callsites later.
                            texture_param_id = i;

                            // Record a new u32 param to this function to add in a later ctx.Replace
                            // step.
                            new_param = b.Param(b.Sym(), b.ty.u32());
                            // iter->second.push_back(new_param);

                            iter->second.emplace(p, new_param);

                            // iter->second.push_back(p);
                            // iter->second.push_back(ctx.Clone(p));

                            var_to_replace.emplace(var, new_param);

                            break;
                        }
                    }
                    TINT_ASSERT(Transform, new_param != nullptr);
                    // TINT_ASSERT(Transform, i < parent_fn_sem->Declaration()->params.Length());

                    // For each callsite, if the passed in argument is a global variable
                    // Get the binding point, add it to bindingpoint_to_data.
                    // If not, i.e. still a function param, keep backtracking until finding the
                    // global variable texture passed in.
                    for (auto* parent_fn_call : parent_fn_sem->CallSites()) {
                        // The value expression can either be a global texture variable, or function
                        // parameter.
                        auto* t = parent_fn_call->Arguments()[texture_param_id]->RootIdentifier();
                        TINT_ASSERT(Transform, t != nullptr);
                        texture_sems_queue.push({t, parent_fn_call->Stmt()->Function()});

                        auto it = call_to_data.find(parent_fn_call);
                        if (it == call_to_data.end()) {
                            it = call_to_data.insert({parent_fn_call, {}}).first;
                        }
                        // temp

                        auto* t_global_var = tint::As<sem::GlobalVariable>(t);
                        if (t_global_var) {
                            printf("\ninsert global var\n");
                            it->second.push_back(CallArgInfo{
                                t_global_var->BindingPoint().value(),
                                0  // placeholder, not used
                            });
                        } else {
                            // temp
                            // TODO: push iter->second.last()
                            printf("\ninsert param\n");
                            // TODO: This is wrong, it's parent of parent_fn
                            // size_t extra_param_id =
                            //     iter->second.size() + parent_fn_params_length - 1;

                            const ast::Parameter* tex_param = nullptr;
                            for (auto* param :
                                 parent_fn_call->Stmt()->Function()->Declaration()->params) {
                                if (param->As<Variable>() == t->Declaration()) {
                                    tex_param = param;
                                    break;
                                }
                            }
                            TINT_ASSERT(Transform, tex_param != nullptr);

                            it->second.push_back(CallArgInfo{std::nullopt, tex_param});

                            // var_to_param_id.emplace(var, extra_param_id);
                        }
                    }

                    // child_fn_sem = parent_fn_sem;
                }
            }

            // if (auto binding = var->BindingPoint()) {
            for (const BindingPoint& binding : bindings) {
                auto iter = bindpoint_to_data.find(binding);
                if (iter == bindpoint_to_data.end()) {
                    printf("\n transform: iterate: binding (%u, %u) \n\n\n", binding.group,
                           binding.binding);

                    TextureBuiltinsFromUniformOptions::DataType dataType;
                    switch (builtin_function_type) {
                        case builtin::Function::kTextureNumLevels:
                            dataType =
                                TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels;
                            break;
                        case builtin::Function::kTextureNumSamples:
                            dataType =
                                TextureBuiltinsFromUniformOptions::DataType::TextureNumSamples;
                            break;
                        default:
                            TINT_UNREACHABLE(Transform, b.Diagnostics())
                                << "unsupported builtin function type " << builtin_function_type;
                    }

                    // For one texture type, it could only be passed into one of the
                    // textureNumLevels or textureNumSamples. So there cannot be duplicate entry for
                    // both dataType.
                    uint32_t index = bindpoint_to_data.size();
                    bindpoint_to_data.emplace(
                        binding, Result::DataEntry{dataType, index * sizeof(uint32_t)});
                }
            }
        });

        // // For each call site, find
        // for (const auto& pair : call_to_data) {
        // }

        if (bindpoint_to_data.empty()) {
            return SkipTransform;
        }

        // Get (or create, on first call) the uniform buffer that will receive the
        // size of each storage buffer in the module.
        const Variable* buffer_size_ubo = nullptr;
        auto get_ubo_sym = [&] {
            if (!buffer_size_ubo) {
                // Emit an array<vec4<u32>, N>, where N is 1/4 number of elements.
                // We do this because UBOs require an element stride that is 16-byte
                // aligned.
                auto* new_member = b.Member(
                    kTextureBuiltinValuesMemberName,
                    b.ty.array(b.ty.vec4(b.ty.u32()), u32((bindpoint_to_data.size() / 4) + 1)));

                // First try get from global variable
                for (auto* var : src->AST().Globals<Var>()) {
                    if (var->HasBindingPoint()) {
                        auto* global_sem = src->Sem().Get<sem::GlobalVariable>(var);

                        // The original binding point
                        BindingPoint binding_point = *global_sem->BindingPoint();

                        if (binding_point == cfg->ubo_binding) {
                            // This ubo_binding struct already exists.
                            // which is added by other *FromUniform transforms.
                            // Replace it with a new struct including the new_member.
                            // Then remove the old structure global declaration.

                            // return var->As<Variable>();
                            buffer_size_ubo = var->As<Variable>();

                            auto* ty = global_sem->Type()->UnwrapRef();
                            auto* str = ty->As<sem::Struct>();
                            if (TINT_UNLIKELY(!str)) {
                                TINT_ICE(Transform, b.Diagnostics())
                                    << "existing ubo binding " << cfg->ubo_binding
                                    << " is not a struct.";
                                // return Program(std::move(b));
                            }

                            utils::Vector<const StructMember*, 4> new_members(
                                ctx.Clone(str->Declaration()->members));
                            new_members.Push(new_member);

                            auto* new_str = b.Structure(b.Symbols().New(), new_members,
                                                        ctx.Clone(str->Declaration()->attributes));
                            ctx.Replace(buffer_size_ubo->type.expr, b.Expr(new_str->name->symbol));
                            ctx.Remove(ctx.src->AST().GlobalDeclarations(), str->Declaration());

                            // return buffer_size_ubo;
                            return ctx.Clone(buffer_size_ubo->name->symbol);
                        }
                    }
                }

                auto* buffer_size_struct = b.Structure(b.Sym(), utils::Vector{
                                                                    new_member,
                                                                });
                buffer_size_ubo = b.GlobalVar(b.Sym(), b.ty.Of(buffer_size_struct),
                                              builtin::AddressSpace::kUniform,
                                              b.Group(AInt(cfg->ubo_binding.group)),
                                              b.Binding(AInt(cfg->ubo_binding.binding)));
            }
            return buffer_size_ubo->name->symbol;
        };

        if (!fn_to_data.empty()) {
            for (const auto& pair : fn_to_data) {
                // for (auto* p : pair.first->Parameters()) {
                // }
                auto* fn = pair.first;
                for (auto t_p : pair.second) {
                    // pair.first->AddParameter(new_p);

                    ctx.InsertAfter(fn->Declaration()->params, fn->Declaration()->params.Back(),
                                    t_p.second);
                }

                printf("\n %s: params length %lu, extra size: %lu \n\n\n",
                       fn->Declaration()->name->symbol.Name().c_str(),
                       fn->Declaration()->params.Length(), pair.second.size());
            }
            for (const auto& pair : call_to_data) {
                // for (auto* p : pair.first->Parameters()) {
                // }
                auto* call = pair.first;

                for (const auto& new_arg_info : pair.second) {
                    // First replace call arg tex param to extra passed in parameter

                    // pair.first->AddParameter(new_p);
                    if (new_arg_info.binding.has_value()) {
                        // This texture is a global variable with binding point.
                        // Read builtin value from uniform buffer.

                        // TODO: de-dup
                        uint32_t index =
                            bindpoint_to_data.at(*new_arg_info.binding).second / sizeof(uint32_t);
                        // auto it = bindpoint_to_data.find(*s.binding);
                        // if (it == bindpoint_to_data.end()) {
                        //     TINT_UNREACHABLE(Transform, b.Diagnostics())
                        //         << "binding type unfound " << *s.binding;
                        // }
                        // // Convert byte offset to index.
                        // uint32_t index = it->second.second / sizeof(uint32_t);

                        // Load the texture builtin value from the UBO.
                        uint32_t array_index = index / 4;
                        auto* vec_expr = b.IndexAccessor(
                            b.MemberAccessor(get_ubo_sym(), kTextureBuiltinValuesMemberName),
                            u32(array_index));
                        uint32_t vec_index = index % 4;

                        auto* builtin_value = b.IndexAccessor(vec_expr, u32(vec_index));
                        ctx.InsertAfter(call->Declaration()->args, call->Declaration()->args.Back(),
                                        builtin_value);
                    } else {
                        // Otherwise this value comes from a function param
                        // ctx.InsertAfter(call->Declaration()->args,
                        // call->Declaration()->args.Back(), b.Expr(u32(9)));

                        auto* parent_fn_sem = call->Stmt()->Function();
                        auto* param = fn_to_data[parent_fn_sem][new_arg_info.tex_param];
                        ctx.InsertAfter(call->Declaration()->args, call->Declaration()->args.Back(),
                                        b.Expr(param->As<Variable>()));
                    }
                }
            }
        }

        // Replace the texture builtin calls with retreiving values from uniform buffer or a
        // function param.

        // TODO: use for builtin_to_replace_param and Replace instead
        IterateInterestedTextureBuiltinsCall([&](const sem::Function* parent_fn_sem,
                                                 const CallExpression* call_expr,
                                                 //    const sem::VariableUser* storage_buffer_sem,
                                                 builtin::Function builtin_function_type,
                                                 //  const sem::GlobalVariable* var) {
                                                 const sem::Variable* texture_sem) {
            auto* global_var = tint::As<sem::GlobalVariable>(texture_sem);
            if (!global_var) {
                // texture is a function parameter.

                // auto iter = builtin_to_replace_param.find(call_expr);
                // TINT_ASSERT(Transform, iter != builtin_to_replace_param.end());

                // auto v = ctx.Clone(iter->second->Declaration()->As<ast::Variable>());
                // ctx.Replace(call_expr, v);

                auto* var = tint::As<sem::Variable>(texture_sem);
                auto iter = var_to_replace.find(var);
                if (iter != var_to_replace.end()) {
                    // auto iter = var_to_param_id.find(var);
                    // if (iter != var_to_param_id.end()) {
                    // ctx.Replace(call_expr,
                    //     b.Expr(parent_fn_sem->Declaration()->params[iter->second]->As<Variable>())
                    // );

                    printf("\n !!!! Found texture var replace param: %s %s \n\n\n",
                           var->Declaration()->name->symbol.Name().c_str(),
                           iter->second->As<Variable>()->name->symbol.Name().c_str());
                    // ctx.Replace(var->Declaration(), ctx.Clone(iter->second));
                    ctx.Replace(call_expr, b.Expr(iter->second->As<Variable>()));
                } else {
                    printf("\ndidn't found\n");
                }

                return;
            }

            // texture is a global variable.
            auto binding = global_var->BindingPoint();
            TINT_ASSERT(Transform, binding);
            // if (!binding) {
            //     return;
            // }

            auto iter = bindpoint_to_data.find(*binding);
            if (iter == bindpoint_to_data.end()) {
                TINT_UNREACHABLE(Transform, b.Diagnostics()) << "binding type unfound " << *binding;
            }

            // Convert byte offset to index.
            uint32_t index = iter->second.second / sizeof(uint32_t);

            // Load the texture num levels data from the UBO.
            uint32_t array_index = index / 4;
            auto* vec_expr = b.IndexAccessor(
                b.MemberAccessor(get_ubo_sym(), kTextureBuiltinValuesMemberName), u32(array_index));
            uint32_t vec_index = index % 4;

            // TODO: builtin_value is either from uniform buffer array
            // or from a passed in function param (recorded in a lookup table)
            auto* builtin_value = b.IndexAccessor(vec_expr, u32(vec_index));

            ctx.Replace(call_expr, builtin_value);
        });

        outputs.Add<Result>(bindpoint_to_data);

        ctx.Clone();
        return Program(std::move(b));
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
    CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};

    /// Iterate over all textureNumLevels() builtins that operate on
    /// storage buffer variables.
    /// @param functor of type void(const CallExpression*, const
    /// sem::VariableUser, const sem::GlobalVariable*). It takes in an
    /// CallExpression of the arrayLength call expression node, a
    /// sem::VariableUser of the used storage buffer variable, and the
    /// sem::GlobalVariable for the storage buffer.
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

            if (builtin->Type() != builtin::Function::kTextureNumLevels &&
                builtin->Type() != builtin::Function::kTextureNumSamples) {
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

            // Get the storage buffer that contains the runtime array.
            // Since we require SimplifyPointers, we can assume that the arrayLength()
            // call has one of two forms:
            //   arrayLength(&struct_var.array_member)
            //   arrayLength(&array_var)

            // auto* param = call_expr->args[0]->As<UnaryOpExpression>();
            // if (TINT_UNLIKELY(!param || param->op != UnaryOp::kAddressOf)) {
            //     TINT_ICE(Transform, b.Diagnostics())
            //         << "expected form of arrayLength argument to be &array_var or "
            //            "&struct_var.array_member";
            //     break;
            // }

            auto* texture_expr = call_expr->args[0];
            // auto* storage_buffer_expr = param->expr;
            // if (auto* accessor = param->expr->As<MemberAccessorExpression>()) {
            //     storage_buffer_expr = accessor->object;
            // }

            // auto* storage_buffer_sem = sem.Get<sem::VariableUser>(storage_buffer_expr);
            // if (TINT_UNLIKELY(!storage_buffer_sem)) {
            //     TINT_ICE(Transform, b.Diagnostics())
            //         << "expected form of arrayLength argument to be &array_var or "
            //            "&struct_var.array_member";
            //     break;
            // }

            // Get the index to use for the buffer size array.
            // auto* var = tint::As<sem::GlobalVariable>(storage_buffer_sem->Variable());
            // auto* storage_buffer_sem = sem.Get<sem::Variable>(storage_buffer_expr);

            auto* texture_sem = sem.GetVal(texture_expr)->RootIdentifier();

            // auto* storage_buffer_sem = sem.Get<sem::VariableUser>(storage_buffer_expr);
            // auto* storage_buffer_sem = sem.Get<sem::ValueExpression>(storage_buffer_expr);
            if (TINT_UNLIKELY(!texture_sem)) {
                TINT_ICE(Transform, b.Diagnostics()) << "expected form of xxxxxxxxxx ";
                break;
            }
            // printf("\n%s\n", storage_buffer_sem.);
            // auto* global_var = tint::As<sem::GlobalVariable>(texture_sem);
            // functor(call_expr, builtin->Type(), global_var);

            // auto* var = tint::As<sem::GlobalVariable>(storage_buffer_sem->Variable());

            // functor(call_expr, builtin->Type(), texture_sem);
            functor(call->Stmt()->Function(), call_expr, builtin->Type(), texture_sem);

            // if (global_var) {
            //     functor(call_expr, builtin->Type(), global_var);
            //     continue;
            // } else {
            //     // Otherwise could be a function parameter. needs to modify function to pass in
            //     the
            //     // builtin values
            //     // 1. Add new

            //     // temp
            //     TINT_ICE(Transform, b.Diagnostics()) << "texture is not a global variable";
            //     break;
            // }

            // if (TINT_UNLIKELY(!var)) {
            //     TINT_ICE(Transform, b.Diagnostics()) << "texture is not a global variable";
            //     break;
            // }

            // functor(call_expr, storage_buffer_sem, var);
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

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

        // std::unordered_map<BindingPoint, uint32_t> bindpoint_to_index;

        // Note: binding here is binding number, it needs conversion to binding index to use for
        // dawn.
        Result::BindingPointDataInfo bindpoint_to_data;

        // Determine the size of the texture_builtin_values array.
        uint32_t builtin_values_count = 0;
        IterateTextureNumLevelsCall(
            [&](const CallExpression*, builtin::Function, const sem::GlobalVariable* var) {
                if (auto binding = var->BindingPoint()) {
                    builtin_values_count++;
                }
            });

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
                    b.ty.array(b.ty.vec4(b.ty.u32()), u32((builtin_values_count / 4) + 1)));

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

        // TODO: fn (tex: texture_2d) {
        //      return textureNumLevels(tex);
        // }
        uint32_t current_index = 0;
        IterateTextureNumLevelsCall([&](const CallExpression* call_expr,
                                        //    const sem::VariableUser* storage_buffer_sem,
                                        builtin::Function builtinFunctionType,
                                        const sem::GlobalVariable* var) {
            // why always combined sampler binidng even before combine sampler transform???
            auto binding = var->BindingPoint();
            if (!binding) {
                return;
            }

            printf("\n transform: iterate: binding (%u, %u) \n\n\n", binding->group,
                   binding->group);

            TextureBuiltinsFromUniformOptions::DataType dataType;
            switch (builtinFunctionType) {
                case builtin::Function::kTextureNumLevels:
                    dataType = TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels;
                    break;
                case builtin::Function::kTextureNumSamples:
                    dataType = TextureBuiltinsFromUniformOptions::DataType::TextureNumSamples;
                    break;
                default:
                    TINT_UNREACHABLE(Transform, b.Diagnostics())
                        << "unsupported builtin function type " << builtinFunctionType;
            }

            uint32_t index = current_index++;
            // bindpoint_to_index.emplace(*binding, size_index);
            // bindpoint_to_data.emplace(*binding,
            //                           Result::DataEntry{dataType, index * sizeof(uint32_t)});

            // TODO: direct binding is combine sampler binding
            // needs original texture binding
            bindpoint_to_data.emplace(*binding,
                                      Result::DataEntry{dataType, index * sizeof(uint32_t)});

            // Load the texture num levels data from the UBO.
            uint32_t array_index = index / 4;
            auto* vec_expr = b.IndexAccessor(
                b.MemberAccessor(get_ubo_sym(), kTextureBuiltinValuesMemberName), u32(array_index));
            uint32_t vec_index = index % 4;

            // ? Calculate actual num levels related to texture view params (or just original
            // texture num levels) texture view: mipLevelCount - baseMipLevel
            auto* texture_num_levels = b.IndexAccessor(vec_expr, u32(vec_index));

            ctx.Replace(call_expr, texture_num_levels);
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
    void IterateTextureNumLevelsCall(F&& functor) {
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
                    // arrayLength() is used as a statement.
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

            auto* storage_buffer_expr = call_expr->args[0];
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

            auto* texture_sem = sem.GetVal(storage_buffer_expr)->RootIdentifier();

            // auto* storage_buffer_sem = sem.Get<sem::VariableUser>(storage_buffer_expr);
            // auto* storage_buffer_sem = sem.Get<sem::ValueExpression>(storage_buffer_expr);
            if (TINT_UNLIKELY(!texture_sem)) {
                TINT_ICE(Transform, b.Diagnostics()) << "expected form of xxxxxxxxxx ";
                break;
            }
            // printf("\n%s\n", storage_buffer_sem.);
            auto* var = tint::As<sem::GlobalVariable>(texture_sem);
            // auto* var = tint::As<sem::GlobalVariable>(storage_buffer_sem->Variable());

            if (TINT_UNLIKELY(!var)) {
                TINT_ICE(Transform, b.Diagnostics()) << "texture is not a global variable";
                break;
            }
            // functor(call_expr, storage_buffer_sem, var);
            functor(call_expr, builtin->Type(), var);
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

// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/dxil/writer/writer.h"

#include "DxilConvPasses/DxilCleanup.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HLSL/HLMatrixType.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HlslIntrinsicOp.h"
#include "llvm/Analysis/ReducibilityAnalysis.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

namespace tint::dxil::writer {
namespace {

class Impl {
  public:
    Impl() {}

    void Build() {
        // Add entry function declaration.
        dxil_mod_.SetEntryFunctionName("main");
        llvm::FunctionType* pEntryFuncType =
            llvm::FunctionType::get(llvm::Type::getVoidTy(llvm_ctx_), false);
        llvm::Function* pFunction =
            llvm::Function::Create(pEntryFuncType, llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                                   dxil_mod_.GetEntryFunctionName(), &llvm_mod_);
        pFunction->setCallingConv(llvm::CallingConv::C);
        dxil_mod_.SetEntryFunction(pFunction);

        // Create main entry function.
        llvm::BasicBlock* bb = llvm::BasicBlock::Create(llvm_ctx_, "entry", pFunction);
        llvm::IRBuilder<> builder(bb);
    }

    void Optimize() {
        llvm::legacy::PassManager pass_mgr;

        TINT_ASSERT(!llvm::verifyModule(llvm_mod_));

        // Verify that CFG is reducible.
        TINT_ASSERT(llvm::IsReducible(llvm_mod_, llvm::IrreducibilityAction::ThrowException));

        pass_mgr.add(llvm::createDxilCleanupPass());
        pass_mgr.run(llvm_mod_);

        TINT_ASSERT(!llvm::verifyModule(llvm_mod_));
    }

  private:
    llvm::LLVMContext llvm_ctx_;                                                       // m_Ctx
    llvm::Module llvm_mod_{"main", llvm_ctx_};                                         // m_pModule
    hlsl::DxilModule& dxil_mod_ = llvm_mod_.GetOrCreateDxilModule(/*skipInit*/ true);  // m_pPR
    hlsl::OP* dxil_op_ = dxil_mod_.GetOP();                                            // m_pOP
};

}  // namespace

Result<Output> Generate(core::ir::Module& ir, const Options& options) {
    Impl impl;
}

}  // namespace tint::dxil::writer

// Copyright 2019 The Dawn Authors
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
#ifndef DAWNNATIVE_METAL_COMMANDRECORDINGCONTEXT_H_
#define DAWNNATIVE_METAL_COMMANDRECORDINGCONTEXT_H_

#include "common/Assert.h"

#import <Metal/Metal.h>

namespace dawn_native { namespace metal {

    class CommandRecordingContext {
      public:
        CommandRecordingContext() = default;
        CommandRecordingContext(id<MTLCommandBuffer> commands) : mCommands([commands retain]) {
        }
        ~CommandRecordingContext() {
            if (mCommands != nil) {
                [mCommands release];
            }
        }

        CommandRecordingContext(CommandRecordingContext&& rhs) : mCommands(rhs.mCommands) {
            ASSERT(!rhs.mInEncoder);
            rhs.mCommands = nil;
        }

        CommandRecordingContext& operator=(CommandRecordingContext&& rhs) {
            ASSERT(!rhs.mInEncoder);
            mCommands = rhs.mCommands;
            rhs.mCommands = nil;
            return *this;
        }

        id<MTLCommandBuffer> GetCommands() {
            return mCommands;
        }

        id<MTLCommandBuffer> AcquireCommands() {
            ASSERT(!mInEncoder);

            id<MTLCommandBuffer> commands = mCommands;
            mCommands = nil;
            return commands;
        }

        id<MTLBlitCommandEncoder> EnsureBlit() {
            ASSERT(mCommands != nil);
            ASSERT(!mInEncoder || mBlit != nil);

            if (mBlit == nil) {
                mInEncoder = true;
                mBlit = [mCommands blitCommandEncoder];
            }
            return mBlit;
        }

        void EndBlit() {
            ASSERT(mCommands != nil);

            if (mBlit != nil) {
                [mBlit endEncoding];
                mBlit = nil;  // This will be autoreleased.
                mInEncoder = false;
            }
        }

        id<MTLComputeCommandEncoder> BeginCompute() {
            ASSERT(mCommands != nil);
            ASSERT(mCompute == nil);
            ASSERT(!mInEncoder);

            mInEncoder = true;
            mCompute = [mCommands computeCommandEncoder];
            return mCompute;
        }

        void EndCompute() {
            ASSERT(mCommands != nil);
            ASSERT(mCompute != nil);

            [mCompute endEncoding];
            mCompute = nil;  // This will be autoreleased.
            mInEncoder = false;
        }

        id<MTLRenderCommandEncoder> BeginRender(MTLRenderPassDescriptor* descriptor) {
            ASSERT(mCommands != nil);
            ASSERT(mRender == nil);
            ASSERT(!mInEncoder);

            mInEncoder = true;
            mRender = [mCommands renderCommandEncoderWithDescriptor:descriptor];
            return mRender;
        }

        void EndRender() {
            ASSERT(mCommands != nil);
            ASSERT(mRender != nil);

            [mRender endEncoding];
            mRender = nil;  // This will be autoreleased.
            mInEncoder = false;
        }

      private:
        id<MTLCommandBuffer> mCommands = nil;
        id<MTLBlitCommandEncoder> mBlit = nil;
        id<MTLComputeCommandEncoder> mCompute = nil;
        id<MTLRenderCommandEncoder> mRender = nil;
        bool mInEncoder = false;
    };

}}  // namespace dawn_native::metal

#endif  // DAWNNATIVE_METAL_COMMANDRECORDINGCONTEXT_H_

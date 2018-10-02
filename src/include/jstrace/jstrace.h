#ifndef JSTRACE_JSTRACE_H_
#define JSTRACE_JSTRACE_H_

#include "dawn/dawn.h"

#include <sstream>

namespace jstrace {

    class Output {
      public:
        template<typename T>
        Output& operator<< (T&& thing) {
            if (mPaused) {
                return *this;
            }
            mOutput << thing;
            return *this;
        }

        void Pause() {mPaused = true;}
        void Resume() {mPaused = false;}

        std::string GetOutputAndClear() {
            std::string result = mOutput.str();
            mOutput.clear();
            mPaused = false;
            return result;
        }

      private:
        std::ostringstream mOutput;
        bool mPaused = false;
    };

    void Init(dawnDevice device);
    dawnProcTable GetProcs(const dawnProcTable* originalProcs);
    void Teardown();

    Output* GetOutput();

    const char* GetBufferName(dawnBuffer buffer);
    const char* GetTextureName(dawnTexture texture);


}  // namespace jstrace

#endif  // JSTRACE_JSTRACE_H_

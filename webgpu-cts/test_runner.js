// Copyright 2022 The Dawn Authors
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

import { DefaultTestFileLoader } from '../third_party/webgpu-cts/src/common/internal/file_loader.js';
import { prettyPrintLog } from '../third_party/webgpu-cts/src/common/internal/logging/log_message.js';
import { Logger } from '../third_party/webgpu-cts/src/common/internal/logging/logger.js';
import { parseQuery } from '../third_party/webgpu-cts/src/common/internal/query/parseQuery.js';

import { TestWorker } from '../third_party/webgpu-cts/src/common/runtime/helper/test_worker.js';

// The Python-side websockets library has a max payload size of 72638. Set the
// max allowable logs size in a single payload to a bit less than that.
const LOGS_MAX_BYTES = 72000;

var socket;

// Returns a wrapper around `fn` which gets called at most once every `intervalMs`.
// If the wrapper is called when `fn` was called too recently, `fn` is scheduled to
// be called later in the future after the interval passes.
// Returns [ wrappedFn, {start, stop}] where wrappedFn is the rate-limited function,
// and start/stop control whether or not the function is enabled. If it is stopped, calls
// to the fn will no-op. If it is started, calls will be rate-limited, starting from
// the time `start` is called.
function rateLimited(fn, intervalMs) {
  let last = undefined;
  let timer = undefined;
  let serial = 0;
  const wrappedFn = (...args) => {
    if (timer !== undefined || last === undefined) {
      // If there is already a fn call scheduled, or the function is
      // not enabled, return.
      return;
    }
    // Get the current time as a number.
    const now = +new Date();
    const diff = now - last;
    if (diff >= intervalMs) {
      // If the time interval has passed, call the function.
      last = now;
      fn(...args);
    } else if (timer === undefined) {
      // Otherwise, we have called `fn` too recently. To rate-limit it,
      // schedule a future call if one has not already been scheduled.
      let scheduledSerial = serial;
      timer = setTimeout(() => {
        // Clear the timer to indicate nothing is scheduled.
        timer = undefined;
        // Check that the serial is current. stop() has not been called.
        if (scheduledSerial === serial) {
          last = +new Date();
          fn(...args);
        }
      }, intervalMs - diff + 1);
    }
  };
  return [
    wrappedFn,
    {
      start: () => {
        last = +new Date();
      },
      stop: () => {
        ++serial;
        last = undefined;
      },
    }
  ];
}

// Make a wrapper around TestCaseRecorder that sends a rate-limited heartbeat
// to the test harness to indicate the test is still making progress.
function makeRecorderWithHeartbeat(rec, sendHeartbeat) {
  return new Proxy(rec, {
    // Create a wrapper around all methods of the TestCaseRecorder.
    get(_target, prop, receiver) {
      const orig = Reflect.get(...arguments);
      if (typeof orig !== 'function') {
        // Return the original property if it is not a function.
        return orig;
      }
      // console.log(prop);
      return (...args) => {
        sendHeartbeat();
        return orig.call(receiver, ...args)
      }
    }
  });
}

function byteSize(s) {
  return new Blob([s]).size;
}

async function setupWebsocket(port) {
  socket = new WebSocket('ws://127.0.0.1:' + port)
  socket.addEventListener('message', runCtsTestViaSocket);
}

async function runCtsTestViaSocket(event) {
  let input = JSON.parse(event.data);
  runCtsTest(input['q'], input['w']);
}

const [sendHeartbeat, {
  start: beginHeartbeatScope,
  stop: endHeartbeatScope
}] = rateLimited(sendMessageTestHeartbeat, 500);

// function wrapPromiseFrom(prototype, key) {
//   const old = prototype[key];
//   prototype[key] = function (...args) {
//     return new Promise((resolve, reject) => {
//       old.call(this, ...args)
//         .then(resolve)
//         .catch(reject)
//         .finally(sendHeartbeat);
//     });
//   }
// }

// wrapPromiseFrom(GPU.prototype, 'requestAdapter');
// wrapPromiseFrom(GPUAdapter.prototype, 'requestAdapterInfo');
// wrapPromiseFrom(GPUAdapter.prototype, 'requestDevice');
// wrapPromiseFrom(GPUDevice.prototype, 'createRenderPipelineAsync');
// wrapPromiseFrom(GPUDevice.prototype, 'createComputePipelineAsync');
// wrapPromiseFrom(GPUDevice.prototype, 'popErrorScope');
// wrapPromiseFrom(GPUQueue.prototype, 'onSubmittedWorkDone');
// wrapPromiseFrom(GPUBuffer.prototype, 'mapAsync');
// wrapPromiseFrom(GPUShaderModule.prototype, 'compilationInfo');

async function runCtsTest(query, use_worker) {
  const workerEnabled = use_worker;
  const worker = workerEnabled ? new TestWorker(false) : undefined;

  const loader = new DefaultTestFileLoader();
  const filterQuery = parseQuery(query);
  const testcases = await loader.loadCases(filterQuery);

  const expectations = [];

  const log = new Logger();

  for (const testcase of testcases) {
    const name = testcase.query.toString();

    const wpt_fn = async () => {
      sendMessageTestStarted();
      const [rec, res] = log.record(name);

      // beginHeartbeatScope();
      // const recWithHeartbeat = makeRecorderWithHeartbeat(rec, sendHeartbeat);
      const timer = setInterval(() => {
        sendMessageTestHeartbeat(name);
      }, 500);
      if (worker) {
        await worker.run(rec, name, expectations);
      } else {
        await testcase.run(rec, expectations);
      }
      clearInterval(timer);
      // endHeartbeatScope();

      sendMessageTestStatus(res.status, res.timems);
      sendMessageTestLog(res.logs);
      sendMessageTestFinished();
    };
    await wpt_fn();
  }
}

function splitLogsForPayload(fullLogs) {
  let logPieces = [fullLogs]
  // Split the log pieces until they all are guaranteed to fit into a
  // websocket payload.
  while (true) {
    let tempLogPieces = []
    for (const piece of logPieces) {
      if (byteSize(piece) > LOGS_MAX_BYTES) {
        let midpoint = Math.floor(piece.length / 2);
        tempLogPieces.push(piece.substring(0, midpoint));
        tempLogPieces.push(piece.substring(midpoint));
      } else {
        tempLogPieces.push(piece)
      }
    }
    // Didn't make any changes - all pieces are under the size limit.
    if (logPieces.every((value, index) => value == tempLogPieces[index])) {
      break;
    }
    logPieces = tempLogPieces;
  }
  return logPieces
}

function sendMessageTestStarted() {
  socket.send('{"type":"TEST_STARTED"}');
}

function sendMessageTestHeartbeat(name) {
  console.log('heartbeat', name);
  socket.send('{"type":"TEST_HEARTBEAT"}');
}

function sendMessageTestStatus(status, jsDurationMs) {
  socket.send(JSON.stringify({'type': 'TEST_STATUS',
                              'status': status,
                              'js_duration_ms': jsDurationMs}));
}

function sendMessageTestLog(logs) {
  splitLogsForPayload((logs ?? []).map(prettyPrintLog).join('\n\n'))
    .forEach((piece) => {
      socket.send(JSON.stringify({
        'type': 'TEST_LOG',
        'log': piece
      }));
    });
}

function sendMessageTestFinished() {
  socket.send('{"type":"TEST_FINISHED"}');
}

window.runCtsTest = runCtsTest;
window.setupWebsocket = setupWebsocket

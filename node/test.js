const gpu = require("../out/Debug/libdawn");
const log = {
  log: console.log,
  expect: (p) => { p ? console.log("PASS") : console.log("FAIL"); }
};

(async () => {
  const d = gpu.getDevice();
  const q = d.getQueue();

  // ----

  const size = 4;
  const testvalue = 187;
  const src = d.createBuffer({
    size,
    usage: 4 | 8,
  });
  src.setSubData(0, new Uint8Array([testvalue]).buffer);

  // ----

  const dst = d.createBuffer({
    size,
    usage: 1 | 8,
  });

  const c = d.createCommandBuffer({});
  c.copyBufferToBuffer(src, 0, dst, 0, size);

  q.submit([c]);

  let done = false;
  dst.mapReadAsync(0, size, (ab) => {
    const data = new Uint8Array(ab);
    log.expect(data[0] == testvalue);
    log.expect(data[1] == 0);
    log.expect(data[2] == 0);
    log.expect(data[3] == 0);
    done = true;
  });

  log.log("waiting...");

  while (!done) {
    d.flush();
    await wait();
  }
  log.log("done!");
})();

function wait() {
    return new Promise((resolve) => { setTimeout(resolve, 10); });
}

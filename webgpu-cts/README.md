# Running the WebGPU CTS Locally with Chrome

Running the WebGPU CTS locally with Chrome requires a Chromium checkout.

Follow [these instructions](https://www.chromium.org/developers/how-tos/get-the-code/) for checking out
and building Chrome.

At the root of a Chromium checkout, run:
`./content/test/gpu/run_gpu_integration_test.py webgpu_cts --browser=exact --browser-executable=path/to/your/chrome-executable`

If you don't want to build Chrome, you can still run the CTS, by passing the path to an existing Chrome executable to the `--browser-executable` argument.

Useful command-line arguments:
 - `-l`: List all tests that would be run.
 - `--test-filter`: Filter tests. Run `--help` for more information.
 - `--help`: See more options.
 - `--passthrough --show-stdout`: Show browser output. See also `--browser-logging-verbosity`.
 - `--extra-browser-args`: Pass extra args to the browser executable.
 - `--is-backend-validation`: Enable backend validation. TODO: rename this to `--backend-validation`.

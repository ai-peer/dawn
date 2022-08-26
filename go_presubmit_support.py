def RunGoTests(input_api, output_api):
    results = []
    try:
        input_api.subprocess.check_call_out(["go", "test", "./..."],
                                            stdout=input_api.subprocess.PIPE,
                                            stderr=input_api.subprocess.PIPE,
                                            cwd=input_api.PresubmitLocalPath())
    except input_api.subprocess.CalledProcessError as e:
        results.append(output_api.PresubmitError('%s' % (e, )))
    return results

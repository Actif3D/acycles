tmp/console.log shows the browser error is still:

  GET /jobs/1/result 404 (NOT FOUND)
  Job result not available

  I rechecked with server.log: acycles is not crashing now. It renders tmp/scenes/a3s-sample/tmp/render.exr, but it does not emit a Studio-compatible
  result_size payload over stdout. The preview page polls /jobs/<id>/result; that endpoint only works when the renderer sends result bytes through the existing
  SparkTrace stdout protocol.

  I removed the server-side render.exr fallback from /F:/GitRepo/ss343/resources/http-spark/spark/local/jobviews.py, because it caused the earlier repeated-
  result loop and still ended in this 404. py_compile passes.

  Bug report prompt for acycles:

  Title: acycles must emit Studio preview result bytes in --status-messages mode

  Asset3D Studio now launches acycles for preview rendering with SparkTrace-compatible arguments, for example:

  F:\GitRepo\acycles\install\acycles.exe --status-messages --a3d-scene <scene-dir> --device CPU --samples 15 --max-diffuse-bounces 8 --max-transparent-bounces
  8 --image-path <scene-dir>\tmp\render.exr

  Observed:
  - acycles completes and writes render.exr successfully.
  - Asset3D Studio browser preview fails with:
    GET /jobs/1/result 404
    Job result not available
  - Server log also shows:
    Unrecognized process output:
    caused by a blank or unprefixed stdout line.

  Expected:
  In --status-messages mode, acycles should follow the existing SparkTrace stdout protocol and emit the rendered preview result bytes, not only write
  render.exr to disk.

  The server parser expects stdout messages like:

  progress: Rendering complete; total: 1; done: 1; result_size: <N>;
  <N raw EXR bytes>

  The raw bytes must immediately follow the newline after the progress header. The server reads exactly result_size bytes from stdout and stores them as
  JobProgress.result. The preview UI then receives those bytes from /jobs/<id>/result.

  Also, in --status-messages mode, stdout should contain only recognized protocol messages:
  - info: ...
  - progress: ...
  - error: ...

  Blank/unprefixed diagnostic output should go to stderr or be prefixed with info:.

  If progressive preview is not implemented yet, emitting one final result_size payload after render completion is enough for the current Asset3D Studio
  preview flow.
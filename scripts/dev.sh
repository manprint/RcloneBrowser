#!/usr/bin/env bash
# Local Docker dev environment: build, test, e2e and smoke-run without network.
# Usage: scripts/dev.sh <command> [qt64|qt610] [-- <ctest args>]
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/dev.sh <command> [qt64|qt610] [-- <ctest args>]

Commands:
  image     build the Docker image (NEEDS NETWORK; never run it offline)
  build     configure and build into build-docker-<flavor>
  test      run ctest -L unit (arguments after -- are passed to ctest)
  e2e       run ctest -L e2e  (arguments after -- are passed to ctest)
  smoke     start the app offscreen for 10s and check that it stays alive
  fixtures  record rclone fixtures with scripts/record-fixtures.sh
  shell     interactive bash in the container

Flavors: qt64 (default, image rb-build:24.04), qt610 (image rb-build:qt610).
Environment: RB_RCLONE = host rclone binary (default: command -v rclone), >= 1.75.
USAGE
}

if [ $# -eq 0 ]; then usage >&2; exit 64; fi
case "$1" in -h|--help|help) usage; exit 0 ;; esac

CMD=$1; shift
case "$CMD" in
  image|build|test|e2e|smoke|fixtures|shell) ;;
  *) echo "dev.sh: unknown command: $CMD" >&2; usage >&2; exit 64 ;;
esac

FLAVOR=qt64
if [ $# -gt 0 ] && [ "$1" != "--" ]; then FLAVOR=$1; shift; fi
EXTRA=()
if [ $# -gt 0 ]; then
  if [ "$1" != "--" ]; then
    echo "dev.sh: unexpected argument: $1 (pass ctest arguments after --)" >&2
    usage >&2; exit 64
  fi
  shift
  EXTRA=("$@")
fi

PREFIX_ARG=""
case "$FLAVOR" in
  qt64)  IMAGE=rb-build:24.04 ;;
  qt610) IMAGE=rb-build:qt610; PREFIX_ARG="-DCMAKE_PREFIX_PATH=/opt/Qt/6.10.3/gcc_64" ;;
  *) echo "dev.sh: unknown flavor: $FLAVOR" >&2; usage >&2; exit 64 ;;
esac

ROOT=$(git rev-parse --show-toplevel)
BDIR="/src/build-docker-$FLAVOR"

if [ "$CMD" = image ]; then
  echo "dev.sh: building $IMAGE (needs network)"
  exec docker build -t "$IMAGE" -f "$ROOT/docker/$FLAVOR.Dockerfile" "$ROOT/docker/"
fi

# Host rclone: only `version` is ever run on the host (it does not load any config).
RB_RCLONE=${RB_RCLONE:-$(command -v rclone || true)}
if [ -z "$RB_RCLONE" ] || [ ! -e "$RB_RCLONE" ]; then
  echo "dev.sh: rclone >= 1.75 required, found: (none; set RB_RCLONE)" >&2
  exit 2
fi
RB_RCLONE=$(readlink -f "$RB_RCLONE")
VLINE=$("$RB_RCLONE" version 2>/dev/null | head -n1 || true)
if ! [[ $VLINE =~ ^rclone\ v1\.(7[5-9]|[89][0-9])\. ]]; then
  echo "dev.sh: rclone >= 1.75 required, found: $VLINE" >&2
  exit 2
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "dev.sh: image $IMAGE missing; build it with: scripts/dev.sh image $FLAVOR (needs network)" >&2
  exit 3
fi

# Quote the passthrough arguments for the inner bash -c.
QEXTRA=""
if [ ${#EXTRA[@]} -gt 0 ]; then printf -v QEXTRA ' %q' "${EXTRA[@]}"; fi

TTY_ARGS=()
case "$CMD" in
  build)
    INNER="cmake -S /src -B $BDIR -G Ninja -DCMAKE_BUILD_TYPE=Release -DRB_BUILD_TESTS=ON -DRB_TEST_RCLONE=/opt/rclone/rclone $PREFIX_ARG && cmake --build $BDIR -j\"\$(nproc)\""
    ;;
  test) INNER="ctest --test-dir $BDIR -L unit --output-on-failure$QEXTRA" ;;
  e2e)  INNER="ctest --test-dir $BDIR -L e2e --output-on-failure$QEXTRA" ;;
  smoke)
    INNER="export XDG_CONFIG_HOME=\$(mktemp -d); QT_QPA_PLATFORM=offscreen $BDIR/build/rclone-browser > /tmp/smoke.log 2>&1 & pid=\$!; sleep 10; if kill -0 \$pid 2>/dev/null; then echo 'smoke: alive after 10s'; kill \$pid; exit 0; else cat /tmp/smoke.log; exit 1; fi"
    ;;
  fixtures)
    if [ ! -f "$ROOT/scripts/record-fixtures.sh" ]; then
      echo "dev.sh: scripts/record-fixtures.sh not present yet" >&2
      exit 4
    fi
    INNER="bash /src/scripts/record-fixtures.sh /opt/rclone/rclone /src/tests/fixtures/rclone-1.75"
    ;;
  shell) INNER="exec bash"; TTY_ARGS=(-it) ;;
esac

# I-9: the container never sees the host HOME or rclone config; RCLONE_CONFIG points to an empty file.
exec docker run --rm ${TTY_ARGS[@]+"${TTY_ARGS[@]}"} --network none \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp/home -e RCLONE_CONFIG=/tmp/home/rclone-empty.conf \
  -v "$ROOT:/src" -v "$RB_RCLONE:/opt/rclone/rclone:ro" -w /src \
  "$IMAGE" bash -c "mkdir -p /tmp/home && : > /tmp/home/rclone-empty.conf && $INNER"

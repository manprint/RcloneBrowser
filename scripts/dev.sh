#!/usr/bin/env bash
# Local Docker dev environment: build, test, e2e and smoke-run without network.
# Usage: scripts/dev.sh <command> [qt64|qt610] [-- <ctest args>]
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/dev.sh <command> [qt64|qt610] [-- <ctest args>]

Commands:
  image     build the Docker image (NEEDS NETWORK; never run it offline)
  server-images  pull/build the isolated protocol server images (NEEDS NETWORK)
  build     configure and build into build-docker-<flavor>
  test      run ctest -L unit (arguments after -- are passed to ctest)
  e2e       run ctest -L e2e  (arguments after -- are passed to ctest)
  servers   run ctest -L servers against isolated sftp/ftp/webdav/s3 containers on an --internal network
  smoke     start the app offscreen for 10s and check that it stays alive
  fixtures  record rclone fixtures with scripts/record-fixtures.sh
  shell     interactive bash in the container

Flavors: qt64 (default, image rb-build:24.04), qt610 (image rb-build:qt610).
Environment: RB_RCLONE = host rclone binary (default: command -v rclone), >= 1.75.
Environment: RB_RCLONE_WEBUI = rclone webui fork binary for the webui e2e tests (optional)
USAGE
}

if [ $# -eq 0 ]; then usage >&2; exit 64; fi
case "$1" in -h|--help|help) usage; exit 0 ;; esac

CMD=$1; shift
case "$CMD" in
  image|server-images|build|test|e2e|servers|smoke|fixtures|shell) ;;
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
  qt610) IMAGE=rb-build:qt610; PREFIX_ARG="-DCMAKE_PREFIX_PATH=/opt/Qt/6.11.3/gcc_64" ;;
  *) echo "dev.sh: unknown flavor: $FLAVOR" >&2; usage >&2; exit 64 ;;
esac

ROOT=$(git rev-parse --show-toplevel)
BDIR="/src/build-docker-$FLAVOR"

if [ "$CMD" = image ]; then
  echo "dev.sh: building $IMAGE (needs network)"
  exec docker build -t "$IMAGE" -f "$ROOT/docker/$FLAVOR.Dockerfile" "$ROOT/docker/"
fi

# Images of the isolated protocol servers (tests/servers; sub-phase 6.3).
SRV_SFTP_IMAGE=atmoz/sftp:alpine
SRV_FTP_IMAGE=delfer/alpine-ftp-server:latest
SRV_S3_IMAGE=ghcr.io/coollabsio/minio:RELEASE.2025-10-15T17-29-55Z
SRV_WEBDAV_IMAGE=rb-srv-webdav:1

if [ "$CMD" = server-images ]; then
  echo "dev.sh: pulling and building the protocol server images (needs network)"
  docker pull "$SRV_SFTP_IMAGE"
  docker pull "$SRV_FTP_IMAGE"
  docker pull "$SRV_S3_IMAGE"
  exec docker build -t "$SRV_WEBDAV_IMAGE" "$ROOT/tests/servers/webdav"
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

# Optional webui fork: mounted read-only next to the host rclone; only `version` is run on the host.
RB_RCLONE_WEBUI=${RB_RCLONE_WEBUI:-}
WEBUI_MOUNT=()
WEBUI_DEF=-DRB_TEST_RCLONE_WEBUI=
if [ -n "$RB_RCLONE_WEBUI" ]; then
  if [ ! -e "$RB_RCLONE_WEBUI" ]; then
    echo "dev.sh: RB_RCLONE_WEBUI not found: $RB_RCLONE_WEBUI" >&2
    exit 2
  fi
  RB_RCLONE_WEBUI=$(readlink -f "$RB_RCLONE_WEBUI")
  WVLINE=$("$RB_RCLONE_WEBUI" version 2>/dev/null | head -n1 || true)
  if ! [[ $WVLINE =~ ^rclone\ v1\. ]]; then
    echo "dev.sh: RB_RCLONE_WEBUI is not an rclone binary, found: $WVLINE" >&2
    exit 2
  fi
  WEBUI_MOUNT=(-v "$RB_RCLONE_WEBUI:/opt/rclone/rclone-webui:ro")
  WEBUI_DEF=-DRB_TEST_RCLONE_WEBUI=/opt/rclone/rclone-webui
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "dev.sh: image $IMAGE missing; build it with: scripts/dev.sh image $FLAVOR (needs network)" >&2
  exit 3
fi

if [ "$CMD" = servers ]; then
  for img in "$SRV_SFTP_IMAGE" "$SRV_FTP_IMAGE" "$SRV_WEBDAV_IMAGE" "$SRV_S3_IMAGE"; do
    if ! docker image inspect "$img" >/dev/null 2>&1; then
      echo "dev.sh: server image $img missing; run scripts/dev.sh server-images (needs network)" >&2
      exit 3
    fi
  done
fi

# Quote the passthrough arguments for the inner bash -c.
QEXTRA=""
if [ ${#EXTRA[@]} -gt 0 ]; then printf -v QEXTRA ' %q' "${EXTRA[@]}"; fi

TTY_ARGS=()
case "$CMD" in
  build)
    INNER="cmake -S /src -B $BDIR -G Ninja -DCMAKE_BUILD_TYPE=Release -DRB_BUILD_TESTS=ON -DRB_TEST_RCLONE=/opt/rclone/rclone $WEBUI_DEF $PREFIX_ARG && cmake --build $BDIR -j\"\$(nproc)\""
    ;;
  test) INNER="ctest --test-dir $BDIR -L unit --output-on-failure$QEXTRA" ;;
  e2e)
    if [ -z "$RB_RCLONE_WEBUI" ]; then
      echo "dev.sh: RB_RCLONE_WEBUI not set: the webdav-webui e2e test is not registered" >&2
    fi
    INNER="ctest --test-dir $BDIR -L e2e --output-on-failure$QEXTRA"
    ;;
  servers) INNER="ctest --test-dir $BDIR -L servers --output-on-failure$QEXTRA" ;;
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

if [ "$CMD" = servers ]; then
  # I-9: the servers live on a per-run --internal network with no published
  # ports; the client container joins only that network.
  NET=rb-srv-$$
  # shellcheck disable=SC2317  # runs from the EXIT trap
  cleanup() {
    # bash ignores further signals here, but the docker CLI re-arms SIGINT and
    # SIGTERM, so a signal can still cancel one docker call: every step below
    # is retried, and only a "not found" answer ends the network removal.
    trap '' INT TERM HUP
    docker rm -f "$NET-client" "$NET-sftp" "$NET-ftp" "$NET-webdav" "$NET-s3" >/dev/null 2>&1 || true
    # The async client CLI may still be creating its container: let it finish
    # (bounded), then remove whatever it started and retry the network removal.
    if [ -n "${cpid:-}" ]; then
      for _ in $(seq 1 20); do
        kill -0 "$cpid" 2>/dev/null || break
        sleep 0.5
      done
    fi
    local err
    for _ in 1 2 3 4 5 6; do
      docker rm -f "$NET-client" "$NET-sftp" "$NET-ftp" "$NET-webdav" "$NET-s3" >/dev/null 2>&1 || true
      if ! err=$(docker network inspect "$NET" 2>&1 >/dev/null); then
        case $err in *"not found"*|*"No such network"*) break ;; esac
      fi
      # a successful rm is confirmed by the next inspect, without waiting
      docker network rm "$NET" >/dev/null 2>&1 && continue
      sleep 1
    done
  }
  cpid=""
  trap cleanup EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
  docker network create --internal "$NET" >/dev/null
  if [ "$(docker network inspect -f '{{.Internal}}' "$NET")" != true ]; then
    echo "dev.sh: network $NET is not internal" >&2
    exit 5
  fi
  docker run -d --name "$NET-sftp" --network "$NET" --network-alias sftp \
    "$SRV_SFTP_IMAGE" rb:rbpass:::upload >/dev/null
  docker run -d --name "$NET-ftp" --network "$NET" --network-alias ftp \
    -e 'USERS=rb|rbpass|/home/rb|1000' -e ADDRESS=ftp --entrypoint /sbin/tini \
    "$SRV_FTP_IMAGE" -- sh -c 'printf "chroot_local_user=YES\nallow_writeable_chroot=YES\n" >> /etc/vsftpd/vsftpd.conf && exec /bin/start_vsftpd.sh' >/dev/null
  docker run -d --name "$NET-webdav" --network "$NET" --network-alias webdav \
    "$SRV_WEBDAV_IMAGE" >/dev/null
  docker run -d --name "$NET-s3" --network "$NET" --network-alias s3 \
    -e MINIO_ROOT_USER=rbadmin -e MINIO_ROOT_PASSWORD=rbpass1234 \
    "$SRV_S3_IMAGE" server /data >/dev/null
  # No exec: the EXIT trap must remove the servers and the network.
  rc=0
  # Async + wait: a trapped signal interrupts `wait` at once and the EXIT trap
  # then removes the named client together with the servers and the network.
  docker run --rm --name "$NET-client" --network "$NET" -e RB_SRV=1 \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp/home -e RCLONE_CONFIG=/tmp/home/rclone-empty.conf \
    -v "$ROOT:/src" -v "$RB_RCLONE:/opt/rclone/rclone:ro" \
    ${WEBUI_MOUNT[@]+"${WEBUI_MOUNT[@]}"} -w /src \
    "$IMAGE" bash -c "mkdir -p /tmp/home && : > /tmp/home/rclone-empty.conf && $INNER" &
  cpid=$!
  wait "$cpid" || rc=$?
  exit "$rc"
fi

# I-9: the container never sees the host HOME or rclone config; RCLONE_CONFIG points to an empty file.
exec docker run --rm ${TTY_ARGS[@]+"${TTY_ARGS[@]}"} --network none \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp/home -e RCLONE_CONFIG=/tmp/home/rclone-empty.conf \
  -v "$ROOT:/src" -v "$RB_RCLONE:/opt/rclone/rclone:ro" \
  ${WEBUI_MOUNT[@]+"${WEBUI_MOUNT[@]}"} -w /src \
  "$IMAGE" bash -c "mkdir -p /tmp/home && : > /tmp/home/rclone-empty.conf && $INNER"

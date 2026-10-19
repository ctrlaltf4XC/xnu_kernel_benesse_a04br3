set -e -x

BINDIR="$DSTROOT/$DT_TOOLCHAIN_DIR/usr/bin"

install -d  -m 0755 "$BINDIR"
install -c  -m 0755 \
	"$PROJECT_DIR/migcom.tproj/mig.sh" \
	"$BINDIR"/mig

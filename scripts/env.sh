# Source this to point the build at a GBDK-2020 install outside vendor/.
#   . scripts/env.sh
export GBDK_HOME="${GBDK_HOME:-$(cd "$(dirname "$0")/.." && pwd)/vendor/gbdk}"


OS=$(uname -s)

if [ "$OS" = "Linux" ]; then
    python3 -m venv .venv
elif [[ "$OS" =~ MINGW* ]] || [[ "$OS" =~ CYGWIN* ]]; then
    python -m venv .venv
else
    echo "未知的系統: $OS"
fi

if [ "$OS" = "Linux" ]; then
    source .venv/bin/activate
elif [[ "$OS" =~ MINGW* ]] || [[ "$OS" =~ CYGWIN* ]]; then
    source .venv/Scripts/activate
else
    echo "未知的系統: $OS"
fi

export PIP_TRUSTED_HOST="pypi.org files.pythonhosted.org pypi.python.org"
pip install west

west config manifest.group-filter -- -local_app

west init -l tools
west update

west config manifest.group-filter ""

west zephyr-export

pip install -r zephyr-rtos/scripts/requirements.txt

# Apply local patches to zephyr-rtos
PATCH_DIR="$(pwd)/tools/patchs"
if [ -d "$PATCH_DIR" ]; then
    for patch in "$PATCH_DIR"/*.patch; do
        [ -e "$patch" ] || continue
        if git -C zephyr-rtos apply --reverse --check "$patch" > /dev/null 2>&1; then
            echo "Patch already applied: $(basename "$patch")"
        elif git -C zephyr-rtos apply --check "$patch" > /dev/null 2>&1; then
            git -C zephyr-rtos apply "$patch"
            echo "Patch applied: $(basename "$patch")"
        else
            echo "Failed to apply patch: $(basename "$patch")"
            exit 1
        fi
    done
fi

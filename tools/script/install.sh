
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

west init -l tools
west update

# west zephyr-export

pip install -r zephyr-rtos/scripts/requirements.txt

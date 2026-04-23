
python -m venv .venv

source .venv/Scripts/activate

pip install west

west init -l tools
west update

# west zephyr-export

pip install -r zephyr-rtos/scripts/requirements.txt

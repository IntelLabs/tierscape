


VENV_DIR = .tierscape_venv
PYTHON = $(VENV_DIR)/bin/python3
PIP = $(VENV_DIR)/bin/pip3

ENABLE_NTIER?=0
export ENABLE_NTIER

# Include MASIM-related targets
include Makefile.masim
include Makefile.memcached

.PHONY: setup clean python_setup python_clean run_clean


check_tierscape_setup:
	@if [ ! -f /tmp/tierscape_env.sh ]; then echo "/tmp/tierscape_env.sh not found! Please run 'make setup' first."; exit 1; fi


# =======================================
python_setup: $(VENV_DIR)
	@echo "Installing dependencies..."
	@$(PIP) install -r requirements.txt
	@echo "Setup complete. Virtual environment ready."
	
	

$(VENV_DIR):
	@echo "Creating virtual environment..."
	@python3 -m venv $(VENV_DIR)

python_clean:
	@echo "Removing virtual environment..."
	@rm -rf $(VENV_DIR)
	@echo "Clean complete."

# =======================================

# makesetup target to run setup_tierscape.sh with sudo
setup:
	@echo "Running setup_tierscape.sh with sudo...${ENABLE_NTIER}"
	@mkdir -p logs
	@sudo bash setup_tierscape.sh ${ENABLE_NTIER} > logs/setup_tierscape.log

# =======================================
	

run_clean:
	@sudo rm -rf /tmp/skd_env.sh

clean:
	@sudo rm -rf logs/*
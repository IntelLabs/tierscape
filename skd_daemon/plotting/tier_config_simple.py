"""
Simple tier configuration reader for Python scripts.
Reads the shared tier_config.h file to get tier definitions using TierInfo structure.
"""

import os
import re

def read_tier_config():
    """Read tier configuration from the shared header file."""
    # Find the config file relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_file = os.path.join(script_dir, "../sk_daemon/utils/tier_config.h")
    model_file = os.path.join(script_dir, "../sk_daemon/utils/model.h")
    
    if not os.path.exists(config_file):
        print(f"Warning: tier config file not found at {config_file}")
        return get_default_config()
    
    # Read both config file and model file for constants
    with open(config_file, 'r') as f:
        config_content = f.read()
    
    # Also read model.h for COMPRESSED_TIERS_BASED constant
    model_content = ""
    if os.path.exists(model_file):
        with open(model_file, 'r') as f:
            model_content = f.read()
    
    # Parse the constants from both files
    constants = {}
    all_content = config_content + "\n" + model_content
    const_pattern = r'#define\s+(\w+)\s+(\d+)'
    for match in re.finditer(const_pattern, all_content):
        constants[match.group(1)] = int(match.group(2))
    
    # Parse tier configurations from the TIER_CONFIGS array
    tiers = []
    
    # Extract the array content between { and };
    array_pattern = r'static const TierConfigData TIER_CONFIGS\[\] = \{(.*?)\};'
    array_match = re.search(array_pattern, config_content, re.DOTALL)
    
    if array_match:
        array_content = array_match.group(1)
        # Find individual tier entries, but skip commented lines
        lines = array_content.split('\n')
        for line in lines:
            line = line.strip()
            # Skip empty lines and comment lines
            if not line or line.startswith('//') or '//' in line.split('{')[0]:
                continue
            
            # Look for tier entries in this line
            tier_match = re.search(r'\{([^}]+)\}', line)
            if tier_match:
                tier_line = tier_match.group(1)
                
                # Parse tier parameters: {virt_id, is_compressed, pool_manager, compressor, mem_type, backing_store, is_cpu, tier_latency}
                parts = [p.strip(' ",') for p in tier_line.split(',')]
                if len(parts) >= 8:
                    try:
                        virt_id = parse_id_expression(parts[0], constants)
                        is_compressed = parts[1].lower() == 'true'
                        pool_manager = parts[2].strip('"')
                        compressor = parts[3].strip('"')
                        mem_type = mem_type_to_int(parts[4])
                        backing_store = mem_type_to_int(parts[5])
                        is_cpu = parts[6].lower() == 'true'
                        tier_latency = int(parts[7])
                        
                        tier = {
                            'virt_id': virt_id,
                            'is_compressed': is_compressed,
                            'pool_manager': pool_manager,
                            'compressor': compressor,
                            'mem_type': mem_type,
                            'backing_store': backing_store,
                            'is_cpu': is_cpu,
                            'tier_latency': tier_latency,
                            'compression_ratio': get_compression_ratio(compressor) if is_compressed else 1.0
                        }
                        tiers.append(tier)
                    except (ValueError, IndexError) as e:
                        print(f"Warning: Failed to parse tier config line: {tier_line}")
                        continue
    
    return {
        'tiers': tiers,
        'constants': constants
    }

def mem_type_to_int(mem_type_str):
    """Convert MEM_TYPE enum to integer."""
    mem_type_str = mem_type_str.strip()
    if mem_type_str == 'DRAM':
        return 0
    elif mem_type_str == 'OPTANE':
        return 1
    elif mem_type_str == 'HBM':
        return 2
    elif mem_type_str == 'CXL':
        return 3
    elif mem_type_str == 'COMPRESSED':
        return 4
    else:
        return 0  # Default to DRAM

def get_compression_ratio(compressor):
    """Get compression ratio based on compressor type (matching TierInfo logic)."""
    if compressor == "zsmalloc":
        return 0.2
    elif compressor == "zstd":
        return 0.5
    else:
        return 0.8

def parse_id_expression(expr, constants):
    """Parse expressions like 'COMPRESSED_TIERS_BASED+0'."""
    expr = expr.strip()
    if '+' in expr:
        base_name, offset = expr.split('+')
        base_value = constants.get(base_name.strip(), 0)
        return base_value + int(offset.strip())
    elif '-' in expr:
        base_name, offset = expr.split('-')
        base_value = constants.get(base_name.strip(), 0)
        return base_value - int(offset.strip())
    else:
        # Try to parse as constant name or number
        try:
            return int(expr)
        except ValueError:
            return constants.get(expr, 0)

def get_default_config():
    """Fallback configuration if header file is not found."""
    return {
        'tiers': [
            {
                'virt_id': 0,
                'is_compressed': False,
                'pool_manager': 'na',
                'compressor': 'na',
                'mem_type': 0,  # DRAM
                'backing_store': 0,  # DRAM
                'is_cpu': True,
                'tier_latency': 2,
                'compression_ratio': 1.0
            },
            {
                'virt_id': 1,
                'is_compressed': False,
                'pool_manager': 'na',
                'compressor': 'na',
                'mem_type': 1,  # OPTANE
                'backing_store': 1,  # OPTANE
                'is_cpu': True,
                'tier_latency': 4,
                'compression_ratio': 1.0
            }
        ],
        'constants': {
            'COMPRESSED_TIERS_BASED': 100,
            'DRAM_TIER_COST': 10,
            'OPTANE_TIER_COST': 3
        }
    }

def get_tier_info():
    """Get tier information compatible with existing plotting scripts."""
    config = read_tier_config()
    return config['tiers']

def get_dram_optane_info():
    """Get DRAM/OPTANE info for numastat plotting."""
    config = read_tier_config()
    dram_info = None
    optane_info = None
    
    for tier in config['tiers']:
        if tier['mem_type'] == 0:  # DRAM
            dram_info = tier
        elif tier['mem_type'] == 1:  # OPTANE
            optane_info = tier
    
    return {'DRAM': dram_info, 'OPTANE': optane_info}

# For compatibility with existing scripts
def get_tier_configs():
    """Get tier configs in the format expected by plot_tier_level_stats.py"""
    config = read_tier_config()
    tier_configs = {}
    
    for tier in config['tiers']:
        # Format: [backing_store, pool_manager, compressor]
        tier_configs[tier['virt_id']] = [
            str(tier['backing_store']),
            tier['pool_manager'],
            tier['compressor']
        ]
    
    return tier_configs

if __name__ == "__main__":
    # Test the configuration reader
    config = read_tier_config()
    print("Tier Configuration:")
    for tier in config['tiers']:
        print(f"  Tier {tier['virt_id']}: {tier}")
    print(f"Constants: {config['constants']}")

"""
Simple tier configuration reader for Python scripts.
Reads the shared tier_config.h and model.h files to get tier constants.
"""

import os
import re

def read_constants():
    """Read constants from the header files."""
    # Find the files relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_file = os.path.join(script_dir, "../sk_daemon/utils/tier_config.h")
    model_file = os.path.join(script_dir, "../sk_daemon/utils/model.h")
    
    constants = {}
    
    # Read model.h for basic constants - REQUIRED
    if not os.path.exists(model_file):
        raise FileNotFoundError(f"Required file not found: {model_file}")
    
    with open(model_file, 'r') as f:
        model_content = f.read()
    
    # Parse constants
    const_pattern = r'#define\s+(\w+)\s+(\d+)'
    for match in re.finditer(const_pattern, model_content):
        constants[match.group(1)] = int(match.group(2))
    
    # Read config file - REQUIRED
    if not os.path.exists(config_file):
        raise FileNotFoundError(f"Required file not found: {config_file}")
    
    with open(config_file, 'r') as f:
        config_content = f.read()
    
    # Parse additional constants from config file
    for match in re.finditer(const_pattern, config_content):
        constants[match.group(1)] = int(match.group(2))
    
    # Verify required constants are present
    required_constants = ['FAST_NODE', 'SLOW_NODE', 'COMPRESSED_TIERS_BASE']
    for const in required_constants:
        if const not in constants:
            raise ValueError(f"Required constant '{const}' not found in header files")
    
    return constants

def get_fast_slow_tier_ids():
    """Returns FAST_NODE and SLOW_NODE tier IDs."""
    constants = read_constants()
    return {
        'FAST_NODE': constants['FAST_NODE'],
        'SLOW_NODE': constants['SLOW_NODE']
    }

def get_compressed_tiers():
    """Return compressed tier IDs minus COMPRESSED_TIERS_BASE."""
    constants = read_constants()
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_file = os.path.join(script_dir, "../sk_daemon/utils/tier_config.h")
    
    # Parse the tier_config.h file to find compressed tier configurations
    with open(config_file, 'r') as f:
        config_content = f.read()
    
    compressed_ids = []
    
    # Look for lines like: {COMPRESSED_TIERS_BASE+0, ...
    # Extract the offset (0, 1, 2, etc.)
    compressed_pattern = r'\{COMPRESSED_TIERS_BASE\+(\d+),'
    for match in re.finditer(compressed_pattern, config_content):
        # Skip commented lines
        line_start = config_content.rfind('\n', 0, match.start()) + 1
        line_end = config_content.find('\n', match.end())
        if line_end == -1:
            line_end = len(config_content)
        line = config_content[line_start:line_end].strip()
        
        if not line.startswith('//'):
            compressed_ids.append(int(match.group(1)))
    
    if not compressed_ids:
        raise ValueError("No compressed tier configurations found in tier_config.h")
    
    return sorted(compressed_ids)

def return_ids_nocompressed():
    """Return IDs of non-compressed tiers."""
    constants = read_constants()
    fast_node = constants['FAST_NODE']
    slow_node = constants['SLOW_NODE']
    
    # Non-compressed tiers are the standard FAST_NODE and SLOW_NODE tiers
    non_compressed_ids = [fast_node, slow_node]
    
    return non_compressed_ids

if __name__ == "__main__":
    # Test the functions
    print("Fast/Slow Tier IDs:", get_fast_slow_tier_ids())
    print("Compressed Tier IDs (minus base):", get_compressed_tiers())
    print("Non-compressed Tier IDs:", return_ids_nocompressed())

def get_compressed_tier_absolute_ids():
    """Returns absolute compressed tier IDs."""
    constants = read_constants()
    compressed_base = constants['COMPRESSED_TIERS_BASE']
    compressed_offsets = get_compressed_tiers()
    
    return [compressed_base + offset for offset in compressed_offsets]

def get_complete_compressed_tiers_info():
    """Returns complete information for all compressed tiers."""
    constants = read_constants()
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_file = os.path.join(script_dir, "../sk_daemon/utils/tier_config.h")
    
    # Parse the tier_config.h file to find compressed tier configurations
    with open(config_file, 'r') as f:
        config_content = f.read()
    
    compressed_tiers = []
    
    # Look for lines like: {COMPRESSED_TIERS_BASE+0, "zsmalloc", "lzo", COMPRESSED, FAST_NODE, true, 5},
    # Pattern: {virt_id, pool_manager, compressor, mem_type, backing_store, is_cpu, tier_latency}
    tier_pattern = r'\{COMPRESSED_TIERS_BASE\+(\d+),\s*"([^"]*)",\s*"([^"]*)",\s*(\w+),\s*(\w+),\s*(true|false),\s*(\d+)\}'
    
    for match in re.finditer(tier_pattern, config_content):
        # Skip commented lines
        line_start = config_content.rfind('\n', 0, match.start()) + 1
        line_end = config_content.find('\n', match.end())
        if line_end == -1:
            line_end = len(config_content)
        line = config_content[line_start:line_end].strip()
        
        if not line.startswith('//'):
            offset = int(match.group(1))
            compressed_base = constants['COMPRESSED_TIERS_BASE']
            
            tier_info = {
                'virt_id': compressed_base + offset,
                'offset': offset,
                'pool_manager': match.group(2),
                'compressor': match.group(3),
                'mem_type': match.group(4),
                'backing_store': constants.get(match.group(5), match.group(5)),  # Try to resolve constant
                'is_cpu': match.group(6) == 'true',
                'tier_latency': int(match.group(7))
            }
            compressed_tiers.append(tier_info)
    
    if not compressed_tiers:
        raise ValueError("No compressed tier configurations found in tier_config.h")
    
    return sorted(compressed_tiers, key=lambda x: x['offset'])

def get_complete_uncompressed_tiers_info():
    """Returns complete information for all uncompressed (standard) tiers."""
    constants = read_constants()
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_file = os.path.join(script_dir, "../sk_daemon/utils/tier_config.h")
    
    # Parse the tier_config.h file to find standard tier configurations
    with open(config_file, 'r') as f:
        config_content = f.read()
    
    uncompressed_tiers = []
    
    # Look for lines like: {FAST_NODE, "na", "na", DRAM, FAST_NODE, true, 2},
    # Pattern: {virt_id, pool_manager, compressor, mem_type, backing_store, is_cpu, tier_latency}
    tier_pattern = r'\{(\w+),\s*"([^"]*)",\s*"([^"]*)",\s*(DRAM|OPTANE),\s*(\w+),\s*(true|false),\s*(\d+)\}'
    
    for match in re.finditer(tier_pattern, config_content):
        # Skip commented lines
        line_start = config_content.rfind('\n', 0, match.start()) + 1
        line_end = config_content.find('\n', match.end())
        if line_end == -1:
            line_end = len(config_content)
        line = config_content[line_start:line_end].strip()
        
        if not line.startswith('//') and not 'COMPRESSED_TIERS_BASE' in line:
            virt_id_str = match.group(1)
            virt_id = constants.get(virt_id_str, virt_id_str)  # Try to resolve constant
            
            tier_info = {
                'virt_id': virt_id,
                'virt_id_name': virt_id_str,
                'pool_manager': match.group(2),
                'compressor': match.group(3),
                'mem_type': match.group(4),
                'backing_store': constants.get(match.group(5), match.group(5)),  # Try to resolve constant
                'is_cpu': match.group(6) == 'true',
                'tier_latency': int(match.group(7))
            }
            uncompressed_tiers.append(tier_info)
    
    if not uncompressed_tiers:
        raise ValueError("No uncompressed tier configurations found in tier_config.h")
    
    return sorted(uncompressed_tiers, key=lambda x: x['virt_id'])

if __name__ == "__main__":
    # Test the functions
    print("Constants:", read_constants())
    print("Fast/Slow Tier IDs:", get_fast_slow_tier_ids())
    print("Compressed Tier Offsets:", get_compressed_tiers())
    print("Compressed Tier Absolute IDs:", get_compressed_tier_absolute_ids())
    print("Complete Compressed Tiers Info:", get_complete_compressed_tiers_info())
    print("Complete Uncompressed Tiers Info:", get_complete_uncompressed_tiers_info())

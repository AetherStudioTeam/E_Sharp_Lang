import struct
import sys

def analyze_pe_file(filename):
    print(f"Analyzing PE file: {filename}\n")
    
    with open(filename, 'rb') as f:
        
        dos_header = f.read(64)
        if dos_header[:2] != b'MZ':
            print("Error: Not a valid PE file (missing MZ header)")
            return
        
        pe_offset = struct.unpack('<I', dos_header[60:64])[0]
        print(f"PE offset: 0x{pe_offset:04X}")
        
        
        f.seek(pe_offset)
        pe_sig = f.read(4)
        if pe_sig != b'PE\x00\x00':
            print("Error: Invalid PE signature")
            return
        
        print(f"PE signature: {pe_sig}")
        
        
        machine = struct.unpack('<H', f.read(2))[0]
        num_sections = struct.unpack('<H', f.read(2))[0]
        time_stamp = struct.unpack('<I', f.read(4))[0]
        ptr_symbols = struct.unpack('<I', f.read(4))[0]
        num_symbols = struct.unpack('<I', f.read(4))[0]
        opt_header_size = struct.unpack('<H', f.read(2))[0]
        characteristics = struct.unpack('<H', f.read(2))[0]
        
        print(f"\nCOFF Header:")
        print(f"  Machine: 0x{machine:04X}")
        print(f"  Number of sections: {num_sections}")
        print(f"  Time date stamp: {time_stamp}")
        print(f"  Size of optional header: {opt_header_size}")
        print(f"  Characteristics: 0x{characteristics:04X}")
        
        
        opt_header_start = f.tell()
        magic = struct.unpack('<H', f.read(2))[0]
        print(f"\nOptional Header:")
        print(f"  Magic: 0x{magic:04X}")
        
        if magic == 0x10b:
            print("  PE32 format")
            major_linker = f.read(1)[0]
            minor_linker = f.read(1)[0]
            code_size = struct.unpack('<I', f.read(4))[0]
            init_data_size = struct.unpack('<I', f.read(4))[0]
            uninit_data_size = struct.unpack('<I', f.read(4))[0]
            entry_point = struct.unpack('<I', f.read(4))[0]
            base_of_code = struct.unpack('<I', f.read(4))[0]
            base_of_data = struct.unpack('<I', f.read(4))[0]
            image_base = struct.unpack('<I', f.read(4))[0]
        elif magic == 0x20b:
            print("  PE32+ format (64-bit)")
            major_linker = f.read(1)[0]
            minor_linker = f.read(1)[0]
            code_size = struct.unpack('<I', f.read(4))[0]
            init_data_size = struct.unpack('<I', f.read(4))[0]
            uninit_data_size = struct.unpack('<I', f.read(4))[0]
            entry_point = struct.unpack('<I', f.read(4))[0]
            base_of_code = struct.unpack('<I', f.read(4))[0]
            image_base = struct.unpack('<Q', f.read(8))[0]
        else:
            print(f"Unknown PE format: 0x{magic:04X}")
            return
        
        print(f"  Entry point RVA: 0x{entry_point:08X}")
        print(f"  Base of code RVA: 0x{base_of_code:08X}")
        print(f"  Image base: 0x{image_base:016X}")
        
        
        section_headers = []
        section_start = opt_header_start + opt_header_size
        
        print(f"\nSections:")
        for i in range(num_sections):
            f.seek(section_start + i * 40)
            name = f.read(8).rstrip(b'\x00').decode('utf-8', errors='ignore')
            virt_size = struct.unpack('<I', f.read(4))[0]
            virt_addr = struct.unpack('<I', f.read(4))[0]
            raw_size = struct.unpack('<I', f.read(4))[0]
            raw_offset = struct.unpack('<I', f.read(4))[0]
            f.read(12)  
            charact = struct.unpack('<I', f.read(4))[0]
            
            section_headers.append({
                'name': name,
                'virt_size': virt_size,
                'virt_addr': virt_addr,
                'raw_size': raw_size,
                'raw_offset': raw_offset,
                'characteristics': charact
            })
            
            print(f"  {name}:")
            print(f"    Virtual size: 0x{virt_size:08X}")
            print(f"    Virtual address: 0x{virt_addr:08X}")
            print(f"    Raw size: 0x{raw_size:08X}")
            print(f"    Raw offset: 0x{raw_offset:08X}")
            print(f"    Characteristics: 0x{charact:08X}")
        
        
        idata_section = None
        for section in section_headers:
            if '.idata' in section['name'].lower():
                idata_section = section
                break
        
        if idata_section:
            print(f"\n.idata section found:")
            print(f"  Raw offset: 0x{idata_section['raw_offset']:08X}")
            print(f"  Raw size: 0x{idata_section['raw_size']:08X}")
            print(f"  Virtual address: 0x{idata_section['virt_addr']:08X}")
            
            
            f.seek(opt_header_start + 96)  
            num_rva = struct.unpack('<I', f.read(4))[0]
            data_dir_offset = opt_header_start + 96
            
            print(f"\nData Directories:")
            import_dir_rva = struct.unpack('<I', f.read(4))[0]
            import_dir_size = struct.unpack('<I', f.read(4))[0]
            iat_dir_rva = struct.unpack('<I', f.read(28))[0]  
            iat_dir_size = struct.unpack('<I', f.read(4))[0]
            
            print(f"  Import Table RVA: 0x{import_dir_rva:08X}, Size: {import_dir_size}")
            print(f"  IAT RVA: 0x{iat_dir_rva:08X}, Size: {iat_dir_size}")
            
            
            if import_dir_rva > 0 and idata_section:
                import_rva = import_dir_rva
                import_file_offset = import_rva - idata_section['virt_addr'] + idata_section['raw_offset']
                
                print(f"\nImport Table Analysis:")
                print(f"  Import table RVA: 0x{import_rva:08X}")
                print(f"  Import table file offset: 0x{import_file_offset:08X}")
                
                f.seek(import_file_offset)
                import_desc_size = 20  
                
                
                import_descriptors = []
                while True:
                    desc_data = f.read(20)
                    if len(desc_data) < 20:
                        break
                    
                    orig_first_thunk = struct.unpack('<I', desc_data[0:4])[0]
                    time_stamp = struct.unpack('<I', desc_data[4:8])[0]
                    forwarder_chain = struct.unpack('<I', desc_data[8:12])[0]
                    name_rva = struct.unpack('<I', desc_data[12:16])[0]
                    first_thunk = struct.unpack('<I', desc_data[16:20])[0]
                    
                    if orig_first_thunk == 0 and name_rva == 0 and first_thunk == 0:
                        break
                    
                    import_descriptors.append({
                        'orig_first_thunk': orig_first_thunk,
                        'time_stamp': time_stamp,
                        'forwarder_chain': forwarder_chain,
                        'name_rva': name_rva,
                        'first_thunk': first_thunk
                    })
                    
                    
                    name_offset = name_rva - idata_section['virt_addr'] + idata_section['raw_offset']
                    name_pos = f.tell()
                    f.seek(name_offset)
                    dll_name = ""
                    while True:
                        c = f.read(1)
                        if c == b'\x00' or c == b'':
                            break
                        dll_name += c.decode('ascii', errors='ignore')
                    f.seek(name_pos)
                    
                    print(f"\n  Import Descriptor:")
                    print(f"    OriginalFirstThunk (INT): 0x{orig_first_thunk:08X}")
                    print(f"    Name RVA: 0x{name_rva:08X} -> DLL: {dll_name}")
                    print(f"    FirstThunk (IAT): 0x{first_thunk:08X}")
                    
                    
                    if orig_first_thunk != 0:
                        int_offset = orig_first_thunk - idata_section['virt_addr'] + idata_section['raw_offset']
                        f.seek(int_offset)
                        
                        print(f"    INT entries:")
                        int_entry_num = 0
                        while True:
                            entry = struct.unpack('<I', f.read(4))[0]
                            if entry == 0:
                                print(f"      [{int_entry_num}]: 0x{entry:08X} (NULL terminator)")
                                break
                            
                            if entry & 0x80000000:
                                print(f"      [{int_entry_num}]: 0x{entry:08X} (ordinal: {entry & 0xFFFF})")
                            else:
                                print(f"      [{int_entry_num}]: 0x{entry:08X} (hint/name RVA)")
                                hint_offset = entry - idata_section['virt_addr'] + idata_section['raw_offset']
                                hint_pos = f.tell()
                                f.seek(hint_offset)
                                hint = struct.unpack('<H', f.read(2))[0]
                                func_name = ""
                                while True:
                                    c = f.read(1)
                                    if c == b'\x00' or c == b'':
                                        break
                                    func_name += c.decode('ascii', errors='ignore')
                                f.seek(hint_pos)
                                print(f"        Hint: {hint}, Function: {func_name}")
                            int_entry_num += 1
                    
                    
                    if first_thunk != 0:
                        iat_offset = first_thunk - idata_section['virt_addr'] + idata_section['raw_offset']
                        f.seek(iat_offset)
                        
                        print(f"    IAT entries:")
                        iat_entry_num = 0
                        while True:
                            entry = struct.unpack('<Q' if magic == 0x20b else '<I', f.read(8 if magic == 0x20b else 4))[0]
                            if entry == 0:
                                print(f"      [{iat_entry_num}]: 0x{entry:016X} (NULL terminator)")
                                break
                            print(f"      [{iat_entry_num}]: 0x{entry:016X}")
                            iat_entry_num += 1
        
        
        print(f"\nEntry Point Analysis:")
        print(f"  Entry point RVA: 0x{entry_point:08X}")
        
        
        for section in section_headers:
            if section['virt_addr'] <= entry_point < section['virt_addr'] + section['virt_size']:
                print(f"  Entry point in section: {section['name']}")
                entry_offset = entry_point - section['virt_addr'] + section['raw_offset']
                print(f"  Entry point file offset: 0x{entry_offset:08X}")
                
                f.seek(entry_offset)
                code = f.read(32)
                print(f"  Entry point code:")
                for i in range(0, min(32, len(code)), 16):
                    hex_str = ' '.join(f'{b:02x}' for b in code[i:i+16])
                    ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in code[i:i+16])
                    print(f"    0x{i:04X}: {hex_str}  {ascii_str}")
                break
        else:
            print("  Warning: Entry point not found in any section!")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python analyze_pe.py <pe_file>")
        sys.exit(1)
    
    analyze_pe_file(sys.argv[1])

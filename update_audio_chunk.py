with open("firmware/include/audio_chunk.h", "r") as f:
    content = f.read()

if "micBuffers" not in content:
    content = content.replace("#endif", "\nextern uint8_t micBuffers[10][1024];\nextern uint8_t spkBuffers[10][4096];\n\n#endif")

with open("firmware/include/audio_chunk.h", "w") as f:
    f.write(content)

with open("firmware/src/main.cpp", "r") as f:
    main_content = f.read()

if "uint8_t micBuffers" not in main_content:
    main_content = main_content.replace("// --- Instances des Modules ---", "uint8_t micBuffers[10][1024];\nuint8_t spkBuffers[10][4096];\n\n// --- Instances des Modules ---")

with open("firmware/src/main.cpp", "w") as f:
    f.write(main_content)

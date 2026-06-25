Import("env")
import os

lib_path = os.path.join(env.subst("$PROJECT_DIR"), "lib", "CubeAI", "Lib")
lib_file = os.path.join(lib_path, "NetworkRuntime1020_CM4_GCC.a")

print(">>> CubeAI lib path:", lib_path)
print(">>> .a exists:", os.path.exists(lib_file))

env.Append(
    LINKFLAGS=[
        "-mcpu=cortex-m4",
        "-mthumb",
        "-mfpu=fpv4-sp-d16",
        "-mfloat-abi=hard",
    ]
)

# Method: wrap with --start-group / --end-group to force re-scanning
env.Append(
    _LIBFLAGS=" -Wl,--start-group " + lib_file + " -Wl,--end-group"
)

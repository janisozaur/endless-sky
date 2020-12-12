import os
from SCons.Node.FS import Dir

# Load environment variables, including some that should be renamed.
env = Environment(ENV = os.environ)
if 'CXX' in os.environ:
	env['CXX'] = os.environ['CXX']
if 'CXXFLAGS' in os.environ:
	env.Append(CCFLAGS = os.environ['CXXFLAGS'])
if 'LDFLAGS' in os.environ:
	env.Append(LINKFLAGS = os.environ['LDFLAGS'])

# The Steam runtime has an out-of-date libstdc++, so link it in statically:
if 'SCHROOT_CHROOT_NAME' in os.environ and 'steamrt' in os.environ['SCHROOT_CHROOT_NAME']:
	env.Append(LINKFLAGS = ["-static-libstdc++"])

opts = Variables()
opts.Add(PathVariable("PREFIX", "Directory to install under", "/usr/local", PathVariable.PathIsDirCreate))
opts.Add(PathVariable("DESTDIR", "Destination root directory", "", PathVariable.PathAccept))
opts.Add(EnumVariable("mode", "Compilation mode", "release", allowed_values=("release", "debug", "profile", "emcc")))
opts.Add(EnumVariable("opengl", "Whether to use OpenGL or OpenGL ES", "desktop", allowed_values=("desktop", "gles")))
opts.Add(EnumVariable("music", "Whether to use music", "on", allowed_values=("on", "off")))
opts.Add(EnumVariable("threads", "Whether to use threads", "on", allowed_values=("on", "off")))
opts.Add(PathVariable("BUILDDIR", "Build directory", "build", PathVariable.PathIsDirCreate))
opts.Update(env)

Help(opts.GenerateHelpText(env))

flags = ["-Wall"]
env.Append(CXXFLAGS = ["-std=c++11"])
common_flags = [""]
if env["mode"] != "debug":
	flags += ["-O3"]
if env["mode"] == "debug":
	flags += ["-g"]
if env["mode"] == "profile":
	flags += ["-pg"]
	env.Append(LINKFLAGS = ["-pg"])
if env["mode"] == "emcc":
	if env["music"] != "off":
		print("emcc requires music=off")
		Exit(1)
	if env["opengl"] != "gles":
		print("emcc requires opengl=gles")
		Exit(1)
	if env["threads"] != "off":
		print("emcc requires threads=off")
		Exit(1)
	flags += ["-g4"]
	env['CXX'] = "em++"
	env['CC'] = "emcc"
	env['AR'] = "emar"
	env['RANLIB'] = "emranlib"
	common_flags += [
		"-s", "DISABLE_EXCEPTION_CATCHING=0",
		"-s", "USE_SDL=2",
		"-s", "USE_LIBPNG=1",
		"-s", "USE_LIBJPEG=1",
		"-s", "USE_WEBGL2=1",
		"-s", "ASSERTIONS=2",
		"-s", "DEMANGLE_SUPPORT=1",
		"-s", "GL_ASSERTIONS=1",
		"-s", "MIN_WEBGL_VERSION=2",
		"-s", "EMULATE_FUNCTION_POINTER_CASTS=1",
		"-s", "FETCH=1"
	]
	env.Append(LINKFLAGS = [
		"--source-map-base", "http://localhost:6931/",
		"-s", "WASM_MEM_MAX=2147483648", # 2GB
		"-s", "INITIAL_MEMORY=838860800", # 800MB
		"-s", "ALLOW_MEMORY_GROWTH=1",
		"-s", "EXTRA_EXPORTED_RUNTIME_METHODS=['callMain']",
		"--preload-file", "data",
		"--preload-file", "images",
		"--preload-file", "sounds",
		"--preload-file", "credits.txt",
		"--preload-file", "keys.txt",
		"--preload-file", "recent.txt",
		#"--preload-file", "dummy@saves/dummy",
		#"--emrun",  # useful in dev, but causes hundreds of errors in prod
		"-g4"
	])
	env.Append(LIBS = [
		"idbfs.js"
	]);

env.Append(LIBS = [
	"openal",
	"webp",
	"webpdemux",]);

if env["mode"] != "emcc":
	env.Append(LIBS = [
		"SDL2",
		"png",
		"jpeg",
	]);


if env["music"] == "off":
	flags += ["-DES_NO_MUSIC"]

if env["threads"] == "off":
	flags += ["-DES_NO_THREADS"]
else:
	env.Append(LIBS = [
		"pthread"
	]);

if env["opengl"] == "desktop":
	env.Append(LIBS = [
		"GL",
		"GLEW"
	]);
else:
	env.Append(LIBS = [
		"GLESv2"
	]);
	flags += ["-DES_GLES"]


# Required build flags. If you want to use SSE optimization, you can turn on
# -msse3 or (if just building for your own computer) -march=native.
env.Append(CCFLAGS = flags)
env.Append(CCFLAGS = common_flags)
env.Append(LINKFLAGS = common_flags)


if env["music"] == "on":
	# libmad is not in the Steam runtime, so link it statically:
	if 'SCHROOT_CHROOT_NAME' in os.environ and 'steamrt_scout_i386' in os.environ['SCHROOT_CHROOT_NAME']:
		env.Append(LIBS = File("/usr/lib/i386-linux-gnu/libmad.a"))
	elif 'SCHROOT_CHROOT_NAME' in os.environ and 'steamrt_scout_amd64' in os.environ['SCHROOT_CHROOT_NAME']:
		env.Append(LIBS = File("/usr/lib/x86_64-linux-gnu/libmad.a"))
	else:
		env.Append(LIBS = "mad")


buildDirectory = env["BUILDDIR"] + "/" + env["mode"]
VariantDir(buildDirectory, "source", duplicate = 0)

# Find all source files.
def RecursiveGlob(pattern, dir_name=buildDirectory):
	# Start with source files in subdirectories.
	matches = [RecursiveGlob(pattern, sub_dir) for sub_dir in Glob(str(dir_name)+"/*")
			   if isinstance(sub_dir, Dir)]
	# Add source files in this directory
	matches += Glob(str(dir_name) + "/" + pattern)
	return matches

env.Append(CPPPATH = ['libwebp/src', 'libwebp'])

env.Library("webp", [
	'libwebp/src/dec/alpha_dec.c',
	'libwebp/src/dec/buffer_dec.c',
	'libwebp/src/dec/frame_dec.c',
	'libwebp/src/dec/idec_dec.c',
	'libwebp/src/dec/io_dec.c',
	'libwebp/src/dec/quant_dec.c',
	'libwebp/src/dec/tree_dec.c',
	'libwebp/src/dec/vp8_dec.c',
	'libwebp/src/dec/vp8l_dec.c',
	'libwebp/src/dec/webp_dec.c',

	'libwebp/src/dsp/alpha_processing.c',
	'libwebp/src/dsp/cpu.c',
	'libwebp/src/dsp/dec.c',
	'libwebp/src/dsp/dec_clip_tables.c',
	'libwebp/src/dsp/filters.c',
	'libwebp/src/dsp/lossless.c',
	'libwebp/src/dsp/rescaler.c',
	'libwebp/src/dsp/upsampling.c',
	'libwebp/src/dsp/yuv.c',

	'libwebp/src/utils/bit_reader_utils.c',
	'libwebp/src/utils/color_cache_utils.c',
	'libwebp/src/utils/filters_utils.c',
	'libwebp/src/utils/huffman_utils.c',
	'libwebp/src/utils/quant_levels_dec_utils.c',
	'libwebp/src/utils/rescaler_utils.c',
	'libwebp/src/utils/random_utils.c',
	'libwebp/src/utils/thread_utils.c',
	'libwebp/src/utils/utils.c',])

env.Library("webpdemux", [
	'libwebp/src/demux/demux.c',
	'libwebp/src/demux/anim_decode.c',])

outname = "endless-sky"
if env["mode"] == "emcc":
    outname += ".js"
sky = env.Program(outname, RecursiveGlob("*.cpp", buildDirectory), LIBPATH='.')

if env["mode"] == "emcc":
    env.Command("title.webp", "images/_menu/title.webp", Copy("$TARGET", "$SOURCE"))

def create_data_version_javascript(env, target, source):
	import hashlib
	hash_md5 = hashlib.md5()
	with open(str(source[0]), "rb") as f:
		for chunk in iter(lambda: f.read(4096), b""):
			hash_md5.update(chunk)
	hash = hash_md5.hexdigest()

	with open(str(target[0]), 'w') as f:
		f.write('// autogenerated file, do not edit\n')
		f.write('// This is the md5 hash of the data file expected\n')
		f.write('var endlessSkyDataVersion = "')
		f.write(hash)
		f.write('";\n')

if env["mode"] == "emcc":
	env.Command(
		target="dataversion.js",
		source="endless-sky.data",
		action=create_data_version_javascript)

# Install the binary:
env.Install("$DESTDIR$PREFIX/games", sky)

# Install the desktop file:
env.Install("$DESTDIR$PREFIX/share/applications", "endless-sky.desktop")

# Install app center metadata:
env.Install("$DESTDIR$PREFIX/share/appdata", "endless-sky.appdata.xml")

# Install icons, keeping track of all the paths.
# Most Ubuntu apps supply 16, 22, 24, 32, 48, and 256, and sometimes others.
sizes = ["16x16", "22x22", "24x24", "32x32", "48x48", "128x128", "256x256", "512x512"]
icons = []
for size in sizes:
	destination = "$DESTDIR$PREFIX/share/icons/hicolor/" + size + "/apps/endless-sky.png"
	icons.append(destination)
	env.InstallAs(destination, "icons/icon_" + size + ".png")

# If any of those icons changed, also update the cache.
# Do not update the cache if we're not installing into "usr".
# (For example, this "install" may actually be creating a Debian package.)
if env.get("PREFIX").startswith("/usr/"):
	env.Command(
		[],
		icons,
		"gtk-update-icon-cache -t $DESTDIR$PREFIX/share/icons/hicolor/")

# Install the man page.
env.Command(
	"$DESTDIR$PREFIX/share/man/man6/endless-sky.6.gz",
	"endless-sky.6",
	"gzip -c $SOURCE > $TARGET")

# Install the data files.
def RecursiveInstall(env, target, source):
	rootIndex = len(env.Dir(source).abspath) + 1
	for node in env.Glob(os.path.join(source, '*')):
		if node.isdir():
			name = node.abspath[rootIndex:]
			RecursiveInstall(env, os.path.join(target, name), node.abspath)
		else:
			env.Install(target, node)
RecursiveInstall(env, "$DESTDIR$PREFIX/share/games/endless-sky/data", "data")
RecursiveInstall(env, "$DESTDIR$PREFIX/share/games/endless-sky/images", "images")
RecursiveInstall(env, "$DESTDIR$PREFIX/share/games/endless-sky/sounds", "sounds")
env.Install("$DESTDIR$PREFIX/share/games/endless-sky", "credits.txt")
env.Install("$DESTDIR$PREFIX/share/games/endless-sky", "keys.txt")

# Make the word "install" in the command line do an installation.
env.Alias("install", "$DESTDIR$PREFIX")

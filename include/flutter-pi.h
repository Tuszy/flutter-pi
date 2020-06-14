#ifndef _FLUTTERPI_H
#define _FLUTTERPI_H

#include <limits.h>
#include <linux/input.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 
#include <stdio.h> 

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <EGL/egl.h>
#define  EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#define  GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>
#include <flutter_embedder.h>

#include <modesetting.h>

enum device_orientation {
	kPortraitUp, kLandscapeLeft, kPortraitDown, kLandscapeRight
};

#define ANGLE_FROM_ORIENTATION(o) \
	((o) == kPortraitUp ? 0 : \
	 (o) == kLandscapeLeft ? 90 : \
	 (o) == kPortraitDown ? 180 : \
	 (o) == kLandscapeRight ? 270 : 0)

#define FLUTTER_ROTATION_TRANSFORMATION(deg) ((FlutterTransformation) \
			{.scaleX = cos(((double) (deg))/180.0*M_PI), .skewX  = -sin(((double) (deg))/180.0*M_PI), .transX = 0, \
			 .skewY  = sin(((double) (deg))/180.0*M_PI), .scaleY = cos(((double) (deg))/180.0*M_PI),  .transY = 0, \
			 .pers0  = 0,					.pers1  = 0,					.pers2  = 1})


extern enum device_orientation orientation;
extern int rotation;

typedef enum {
	kVBlankRequest,
	kVBlankReply,
	kUpdateOrientation,
	kSendPlatformMessage,
	kRespondToPlatformMessage,
	kFlutterTask,
	kRegisterExternalTexture,
	kUnregisterExternalTexture,
	kMarkExternalTextureFrameAvailable,
	kGeneric
} flutterpi_task_type;

struct flutterpi_task {
    struct flutterpi_task* next;
	flutterpi_task_type type;
	union {
		FlutterTask task;
		struct {
			uint64_t vblank_ns;
			intptr_t baton;
		};
		enum device_orientation orientation;
		struct {
			char *channel;
			const FlutterPlatformMessageResponseHandle *responsehandle;
			size_t message_size;
			uint8_t *message;
		};
		int64_t texture_id;
		struct {
			void (*callback)(void *userdata);
			void *userdata;
		};
	};
    uint64_t target_time;
};

static inline void *memdup(const void *restrict src, const size_t n) {
	void *__restrict__ dest;

	if ((src == NULL) || (n == 0)) return NULL;

	dest = malloc(n);
	if (dest == NULL) return NULL;

	return memcpy(dest, src, n);
}

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
};

struct pageflip_data {
	struct gbm_bo *releaseable_bo;
	intptr_t next_baton;
};

// position & pointer phase of a mouse pointer / multitouch slot
// A 10-finger multi-touch display has 10 slots and each of them have their own position, tracking id, etc.
// All mouses / touchpads share the same mouse pointer.
struct mousepointer_mtslot {
	// the MT tracking ID used to track this touch.
	int     id;
	int32_t flutter_slot_id;
	double  x, y;
	FlutterPointerPhase phase;
};

extern struct drm {
	//char device[PATH_MAX];
	//bool has_device;
	//int fd;
	//uint32_t connector_id;
	//drmModeModeInfo *mode;
	//uint32_t crtc_id;
	//size_t crtc_index;
	struct gbm_bo *current_bo;
	drmEventContext evctx;
	
	bool disable_vsync;
	struct drmdev *drmdev;
} drm;

extern struct gbm {
	struct gbm_device  *device;
	struct gbm_surface *surface;
	uint32_t 			format;
	uint64_t			modifier;
} gbm;

extern struct egl {
	EGLDisplay display;
	EGLConfig  config;
	EGLContext root_context;
	EGLContext flutter_render_context;
	EGLContext flutter_resource_uploading_context;
	EGLContext vidpp_context;
	EGLContext compositor_context;
	EGLSurface surface;

	bool	   modifiers_supported;
	char      *renderer;

	PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay;
	PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC createPlatformWindowSurface;
	PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC createPlatformPixmapSurface;
	PFNEGLCREATEDRMIMAGEMESAPROC createDRMImageMESA;
	PFNEGLEXPORTDRMIMAGEMESAPROC exportDRMImageMESA;
} egl;

extern struct gl {
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC EGLImageTargetTexture2DOES;
	PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC EGLImageTargetRenderbufferStorageOES;
} gl;

#define LOAD_EGL_PROC(name) \
    do { \
		char proc[256]; \
		snprintf(proc, 256, "%s", "egl" #name); \
		proc[3] = toupper(proc[3]); \
        egl.name = (void*) eglGetProcAddress(proc); \
        if (!egl.name) { \
            fprintf(stderr, "could not resolve EGL procedure " #name "\n"); \
            return EINVAL; \
        } \
    } while (false)

#define LOAD_GL_PROC(name) \
	do { \
		char proc_name[256]; \
		snprintf(proc_name, 256, "gl" #name); \
		proc_name[2] = toupper(proc_name[2]); \
		gl.name = (void*) eglGetProcAddress(proc_name); \
		if (!gl.name) { \
			fprintf(stderr, "could not resolve GL procedure " #name "\n"); \
			return EINVAL; \
		} \
	} while (false)

#define INPUT_BUSTYPE_FRIENDLY_NAME(bustype) ( \
	(bustype) == BUS_PCI ? "PCI/e" : \
	(bustype) == BUS_USB ? "USB" : \
	(bustype) == BUS_BLUETOOTH ? "Bluetooth" : \
	(bustype) == BUS_VIRTUAL ? "virtual" : \
	(bustype) == BUS_I2C ? "I2C" : \
	(bustype) == BUS_HOST ? "Host-Interface" : \
	(bustype) == BUS_SPI ? "SPI" : "other")

#define FLUTTER_BUTTON_FROM_EVENT_CODE(code) ((uint16_t) \
	(code) == BTN_LEFT ? kFlutterPointerButtonMousePrimary : \
	(code) == BTN_RIGHT ? kFlutterPointerButtonMouseSecondary : \
	(code) == BTN_MIDDLE ? kFlutterPointerButtonMouseMiddle : \
	(code) == BTN_FORWARD ? kFlutterPointerButtonMouseForward : \
	(code) == BTN_BACK ? kFlutterPointerButtonMouseBack : \
	(code) == BTN_TOUCH ? (1 << 8) : 0)

#define MODIFIER_KEY_FROM_EVENT_CODE(code) ((uint16_t) \
	((code) == KEY_LEFTCTRL) || ((code) == KEY_RIGHTCTRL) ? kControlModifier : \
	((code) == KEY_LEFTSHIFT) || ((code) == KEY_RIGHTSHIFT) ? kShiftModifier : \
	((code) == KEY_LEFTALT) || ((code) == KEY_RIGHTALT) ? kAltModifier : \
	((code) == KEY_LEFTMETA) || ((code) == KEY_RIGHTMETA) ? kMetaModifier : \
	((code) == KEY_CAPSLOCK) ? kCapsLockModifier : \
	((code) == KEY_NUMLOCK) ? kNumLockModifier : 0)

#define POINTER_PHASE_AS_STRING(phase) ( \
	(phase) == kCancel ? "kCancel" : \
	(phase) == kUp ? "kUp" : \
	(phase) == kDown ? "kDown" : \
	(phase) == kMove ? "kMove" : \
	(phase) == kAdd ? "kAdd" : \
	(phase) == kRemove ? "kRemove" : \
	(phase) == kHover ? "kHover" : "???")

#define ISSET(uint32bitmap, bit) (uint32bitmap[(bit)/32] & (1 << ((bit) & 0x1F)))

struct input_device {
	char path[PATH_MAX];
	char name[256];
	struct input_id input_id;
	int  fd;

	// the pointer device kind reported to the flutter engine
	FlutterPointerDeviceKind kind;

	// this should be true for mouse and touchpad, false for touchscreens / stylus
	bool is_pointer;
	bool is_direct;
	
	// for EV_ABS devices (touchscreens, some touchpads)
	struct input_absinfo xinfo, yinfo;

	// n_slots is > 1 for Multi-Touch devices (most touchscreens)
	// just because n_slots is 0 and slots is NULL, doesn't mean active_slot is NULL.
	// mouse devices own 0 slots (since they all share a global slot), and still have an active_slot.
	size_t n_mtslots;
	size_t i_active_mtslot;
	struct mousepointer_mtslot *mtslots;

	// currently pressed buttons (for mouse, touchpad, stylus)
	// (active_buttons & 0xFF) will be the value of the "buttons" field
	//   of the FlutterPointerEvent being sent to flutter
	uint16_t active_buttons;
};

// we have one global mouse pointer, even if multiple mouses are attached
extern struct mousepointer_mtslot mousepointer;

extern FlutterEngine engine;

struct drm_fb *drm_fb_get_from_bo(struct gbm_bo *bo);

void post_platform_task(struct flutterpi_task *task);

int flutterpi_send_platform_message(const char *channel,
									const uint8_t *restrict message,
									size_t message_size,
									FlutterPlatformMessageResponseHandle *responsehandle);

int flutterpi_respond_to_platform_message(FlutterPlatformMessageResponseHandle *handle,
										  const uint8_t *restrict message,
										  size_t message_size);

#endif
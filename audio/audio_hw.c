<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd"><html><head><title>audio/audio_hw.c - device/asus/grouper - Git at Google</title><link rel="stylesheet" type="text/css" href="//www.google.com/css/go.css" /><link rel="stylesheet" type="text/css" href="/+static/prettify/prettify.vf-M93Ay4IiiWRQSJKPGWQ.cache.css" /><link rel="stylesheet" type="text/css" href="/+static/gitiles.76iNvupRwElh_9ac4itPQg.cache.css" /><script src="/+static/prettify/prettify_compiled.tBfOdWkOBM1HHwscnUdTAA.cache.js" type="text/javascript"></script></head><body onload="prettyPrint()"><h1><img src="//www.google.com/images/logo_sm.gif" alt="Google" />Git</h1><div class="menu"> <a href="https://www.google.com/accounts/ServiceLogin?service=gerritcodereview&amp;continue=https://android.googlesource.com/login/device/asus/grouper/%2B/c384f5f2ac0c60106a93b45c30ff4137131f2d39/audio/audio_hw.c">Sign in</a> </div><div class="breadcrumbs"><a href="/?format=HTML">android</a> / <a href="/device/asus/grouper/">device/asus/grouper</a> / <a href="/device/asus/grouper/+/c384f5f2ac0c60106a93b45c30ff4137131f2d39">c384f5f2ac0c60106a93b45c30ff4137131f2d39</a> / <a href="/device/asus/grouper/+/c384f5f2ac0c60106a93b45c30ff4137131f2d39/">.</a> / <a href="/device/asus/grouper/+/c384f5f2ac0c60106a93b45c30ff4137131f2d39/audio">audio</a> / audio_hw.c</div><div class="sha1">blob: 5a9cbf6b7880d4bb9175decc0cbd9e11ac291de9 [<a href="/device/asus/grouper/+log/c384f5f2ac0c60106a93b45c30ff4137131f2d39/audio/audio_hw.c">file history</a>]</div><pre class="git-blob prettyprint linenums lang-c">/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the &quot;License&quot;);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an &quot;AS IS&quot; BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG &quot;audio_hw_primary&quot;
/*#define LOG_NDEBUG 0*/

#include &lt;errno.h&gt;
#include &lt;pthread.h&gt;
#include &lt;stdint.h&gt;
#include &lt;stdlib.h&gt;
#include &lt;sys/time.h&gt;

#include &lt;cutils/log.h&gt;
#include &lt;cutils/properties.h&gt;
#include &lt;cutils/str_parms.h&gt;

#include &lt;hardware/audio.h&gt;
#include &lt;hardware/hardware.h&gt;

#include &lt;system/audio.h&gt;

#include &lt;tinyalsa/asoundlib.h&gt;

#include &lt;audio_utils/resampler.h&gt;

#include &quot;audio_route.h&quot;

#define PCM_CARD 1
#define PCM_DEVICE 0
#define PCM_DEVICE_SCO 2

#define OUT_PERIOD_SIZE 512
#define OUT_SHORT_PERIOD_COUNT 2
#define OUT_LONG_PERIOD_COUNT 8
#define OUT_SAMPLING_RATE 44100

#define IN_PERIOD_SIZE 1024
#define IN_PERIOD_COUNT 4
#define IN_SAMPLING_RATE 44100

#define SCO_PERIOD_SIZE 256
#define SCO_PERIOD_COUNT 4
#define SCO_SAMPLING_RATE 8000

/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 2000
#define MAX_WRITE_SLEEP_US ((OUT_PERIOD_SIZE * OUT_SHORT_PERIOD_COUNT * 1000000) \
                                / OUT_SAMPLING_RATE)

enum {
    OUT_BUFFER_TYPE_UNKNOWN,
    OUT_BUFFER_TYPE_SHORT,
    OUT_BUFFER_TYPE_LONG,
};

struct pcm_config pcm_config_out = {
    .channels = 2,
    .rate = OUT_SAMPLING_RATE,
    .period_size = OUT_PERIOD_SIZE,
    .period_count = OUT_LONG_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = OUT_PERIOD_SIZE * OUT_SHORT_PERIOD_COUNT,
};

struct pcm_config pcm_config_in = {
    .channels = 2,
    .rate = IN_SAMPLING_RATE,
    .period_size = IN_PERIOD_SIZE,
    .period_count = IN_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 1,
    .stop_threshold = (IN_PERIOD_SIZE * IN_PERIOD_COUNT),
};

struct pcm_config pcm_config_sco = {
    .channels = 1,
    .rate = SCO_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    unsigned int out_device;
    unsigned int in_device;
    bool standby;
    bool mic_mute;
    struct audio_route *ar;
    int orientation;
    bool screen_off;

    struct stream_out *active_out;
    struct stream_in *active_in;
};

struct stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct pcm *pcm;
    struct pcm_config *pcm_config;
    bool standby;

    struct resampler_itfe *resampler;
    int16_t *buffer;
    size_t buffer_frames;

    int write_threshold;
    int cur_write_threshold;
    int buffer_type;

    struct audio_device *dev;
};

struct stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct pcm *pcm;
    struct pcm_config *pcm_config;
    bool standby;

    unsigned int requested_rate;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider buf_provider;
    int16_t *buffer;
    size_t buffer_size;
    size_t frames_in;
    int read_status;

    struct audio_device *dev;
};

enum {
    ORIENTATION_LANDSCAPE,
    ORIENTATION_PORTRAIT,
    ORIENTATION_SQUARE,
    ORIENTATION_UNDEFINED,
};

static uint32_t out_get_sample_rate(const struct audio_stream *stream);
static size_t out_get_buffer_size(const struct audio_stream *stream);
static audio_format_t out_get_format(const struct audio_stream *stream);
static uint32_t in_get_sample_rate(const struct audio_stream *stream);
static size_t in_get_buffer_size(const struct audio_stream *stream);
static audio_format_t in_get_format(const struct audio_stream *stream);
static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer);
static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer);

/*
 * NOTE: when multiple mutexes have to be acquired, always take the
 * audio_device mutex first, followed by the stream_in and/or
 * stream_out mutexes.
 */

/* Helper functions */

static void select_devices(struct audio_device *adev)
{
    int headphone_on;
    int speaker_on;
    int docked;
    int main_mic_on;

    headphone_on = adev-&gt;out_device &amp; (AUDIO_DEVICE_OUT_WIRED_HEADSET |
                                    AUDIO_DEVICE_OUT_WIRED_HEADPHONE);
    speaker_on = adev-&gt;out_device &amp; AUDIO_DEVICE_OUT_SPEAKER;
    docked = adev-&gt;out_device &amp; AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET;
    main_mic_on = adev-&gt;in_device &amp; AUDIO_DEVICE_IN_BUILTIN_MIC;

    reset_mixer_state(adev-&gt;ar);

    if (speaker_on)
        audio_route_apply_path(adev-&gt;ar, &quot;speaker&quot;);
    if (headphone_on)
        audio_route_apply_path(adev-&gt;ar, &quot;headphone&quot;);
    if (docked)
        audio_route_apply_path(adev-&gt;ar, &quot;dock&quot;);
    if (main_mic_on) {
        if (adev-&gt;orientation == ORIENTATION_LANDSCAPE)
            audio_route_apply_path(adev-&gt;ar, &quot;main-mic-left&quot;);
        else
            audio_route_apply_path(adev-&gt;ar, &quot;main-mic-top&quot;);
    }

    update_mixer_state(adev-&gt;ar);

    ALOGV(&quot;hp=%c speaker=%c dock=%c main-mic=%c&quot;, headphone_on ? &#39;y&#39; : &#39;n&#39;,
          speaker_on ? &#39;y&#39; : &#39;n&#39;, docked ? &#39;y&#39; : &#39;n&#39;, main_mic_on ? &#39;y&#39; : &#39;n&#39;);
}

/* must be called with hw device and output stream mutexes locked */
static void do_out_standby(struct stream_out *out)
{
    struct audio_device *adev = out-&gt;dev;

    if (!out-&gt;standby) {
        pcm_close(out-&gt;pcm);
        out-&gt;pcm = NULL;
        adev-&gt;active_out = NULL;
        if (out-&gt;resampler) {
            release_resampler(out-&gt;resampler);
            out-&gt;resampler = NULL;
        }
        if (out-&gt;buffer) {
            free(out-&gt;buffer);
            out-&gt;buffer = NULL;
        }
        out-&gt;standby = true;
    }
}

/* must be called with hw device and input stream mutexes locked */
static void do_in_standby(struct stream_in *in)
{
    struct audio_device *adev = in-&gt;dev;

    if (!in-&gt;standby) {
        pcm_close(in-&gt;pcm);
        in-&gt;pcm = NULL;
        adev-&gt;active_in = NULL;
        if (in-&gt;resampler) {
            release_resampler(in-&gt;resampler);
            in-&gt;resampler = NULL;
        }
        if (in-&gt;buffer) {
            free(in-&gt;buffer);
            in-&gt;buffer = NULL;
        }
        in-&gt;standby = true;
    }
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream(struct stream_out *out)
{
    struct audio_device *adev = out-&gt;dev;
    unsigned int device;
    int ret;

    /*
     * Due to the lack of sample rate converters in the SoC,
     * it greatly simplifies things to have only the main
     * (speaker/headphone) PCM or the BC SCO PCM open at
     * the same time.
     */
    if (adev-&gt;out_device &amp; AUDIO_DEVICE_OUT_ALL_SCO) {
        device = PCM_DEVICE_SCO;
        out-&gt;pcm_config = &amp;pcm_config_sco;
    } else {
        device = PCM_DEVICE;
        out-&gt;pcm_config = &amp;pcm_config_out;
        out-&gt;buffer_type = OUT_BUFFER_TYPE_UNKNOWN;
    }

    /*
     * All open PCMs can only use a single group of rates at once:
     * Group 1: 11.025, 22.05, 44.1
     * Group 2: 8, 16, 32, 48
     * Group 1 is used for digital audio playback since 44.1 is
     * the most common rate, but group 2 is required for SCO.
     */
    if (adev-&gt;active_in) {
        struct stream_in *in = adev-&gt;active_in;
        pthread_mutex_lock(&amp;in-&gt;lock);
        if (((out-&gt;pcm_config-&gt;rate % 8000 == 0) &amp;&amp;
                 (in-&gt;pcm_config-&gt;rate % 8000) != 0) ||
                 ((out-&gt;pcm_config-&gt;rate % 11025 == 0) &amp;&amp;
                 (in-&gt;pcm_config-&gt;rate % 11025) != 0))
            do_in_standby(in);
        pthread_mutex_unlock(&amp;in-&gt;lock);
    }

    out-&gt;pcm = pcm_open(PCM_CARD, device, PCM_OUT | PCM_NORESTART, out-&gt;pcm_config);

    if (out-&gt;pcm &amp;&amp; !pcm_is_ready(out-&gt;pcm)) {
        ALOGE(&quot;pcm_open(out) failed: %s&quot;, pcm_get_error(out-&gt;pcm));
        pcm_close(out-&gt;pcm);
        return -ENOMEM;
    }

    /*
     * If the stream rate differs from the PCM rate, we need to
     * create a resampler.
     */
    if (out_get_sample_rate(&amp;out-&gt;stream.common) != out-&gt;pcm_config-&gt;rate) {
        ret = create_resampler(out_get_sample_rate(&amp;out-&gt;stream.common),
                               out-&gt;pcm_config-&gt;rate,
                               out-&gt;pcm_config-&gt;channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               NULL,
                               &amp;out-&gt;resampler);
        out-&gt;buffer_frames = (pcm_config_out.period_size * out-&gt;pcm_config-&gt;rate) /
                out_get_sample_rate(&amp;out-&gt;stream.common) + 1;

        out-&gt;buffer = malloc(pcm_frames_to_bytes(out-&gt;pcm, out-&gt;buffer_frames));
    }

    adev-&gt;active_out = out;

    return 0;
}

/* must be called with hw device and input stream mutexes locked */
static int start_input_stream(struct stream_in *in)
{
    struct audio_device *adev = in-&gt;dev;
    unsigned int device;
    int ret;

    /*
     * Due to the lack of sample rate converters in the SoC,
     * it greatly simplifies things to have only the main
     * mic PCM or the BC SCO PCM open at the same time.
     */
    if (adev-&gt;in_device &amp; AUDIO_DEVICE_IN_ALL_SCO) {
        device = PCM_DEVICE_SCO;
        in-&gt;pcm_config = &amp;pcm_config_sco;
    } else {
        device = PCM_DEVICE;
        in-&gt;pcm_config = &amp;pcm_config_in;
    }

    /*
     * All open PCMs can only use a single group of rates at once:
     * Group 1: 11.025, 22.05, 44.1
     * Group 2: 8, 16, 32, 48
     * Group 1 is used for digital audio playback since 44.1 is
     * the most common rate, but group 2 is required for SCO.
     */
    if (adev-&gt;active_out) {
        struct stream_out *out = adev-&gt;active_out;
        pthread_mutex_lock(&amp;out-&gt;lock);
        if (((in-&gt;pcm_config-&gt;rate % 8000 == 0) &amp;&amp;
                 (out-&gt;pcm_config-&gt;rate % 8000) != 0) ||
                 ((in-&gt;pcm_config-&gt;rate % 11025 == 0) &amp;&amp;
                 (out-&gt;pcm_config-&gt;rate % 11025) != 0))
            do_out_standby(out);
        pthread_mutex_unlock(&amp;out-&gt;lock);
    }

    in-&gt;pcm = pcm_open(PCM_CARD, device, PCM_IN, in-&gt;pcm_config);

    if (in-&gt;pcm &amp;&amp; !pcm_is_ready(in-&gt;pcm)) {
        ALOGE(&quot;pcm_open(in) failed: %s&quot;, pcm_get_error(in-&gt;pcm));
        pcm_close(in-&gt;pcm);
        return -ENOMEM;
    }

    /*
     * If the stream rate differs from the PCM rate, we need to
     * create a resampler.
     */
    if (in_get_sample_rate(&amp;in-&gt;stream.common) != in-&gt;pcm_config-&gt;rate) {
        in-&gt;buf_provider.get_next_buffer = get_next_buffer;
        in-&gt;buf_provider.release_buffer = release_buffer;

        ret = create_resampler(in-&gt;pcm_config-&gt;rate,
                               in_get_sample_rate(&amp;in-&gt;stream.common),
                               1,
                               RESAMPLER_QUALITY_DEFAULT,
                               &amp;in-&gt;buf_provider,
                               &amp;in-&gt;resampler);
    }
    in-&gt;buffer_size = pcm_frames_to_bytes(in-&gt;pcm,
                                          in-&gt;pcm_config-&gt;period_size);
    in-&gt;buffer = malloc(in-&gt;buffer_size);
    in-&gt;frames_in = 0;

    adev-&gt;active_in = in;

    return 0;
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    if (in-&gt;pcm == NULL) {
        buffer-&gt;raw = NULL;
        buffer-&gt;frame_count = 0;
        in-&gt;read_status = -ENODEV;
        return -ENODEV;
    }

    if (in-&gt;frames_in == 0) {
        in-&gt;read_status = pcm_read(in-&gt;pcm,
                                   (void*)in-&gt;buffer,
                                   in-&gt;buffer_size);
        if (in-&gt;read_status != 0) {
            ALOGE(&quot;get_next_buffer() pcm_read error %d&quot;, in-&gt;read_status);
            buffer-&gt;raw = NULL;
            buffer-&gt;frame_count = 0;
            return in-&gt;read_status;
        }
        in-&gt;frames_in = in-&gt;pcm_config-&gt;period_size;
        if (in-&gt;pcm_config-&gt;channels == 2) {
            unsigned int i;

            /* Discard right channel */
            for (i = 1; i &lt; in-&gt;frames_in; i++)
                in-&gt;buffer[i] = in-&gt;buffer[i * 2];
        }
    }

    buffer-&gt;frame_count = (buffer-&gt;frame_count &gt; in-&gt;frames_in) ?
                                in-&gt;frames_in : buffer-&gt;frame_count;
    buffer-&gt;i16 = in-&gt;buffer + (in-&gt;pcm_config-&gt;period_size - in-&gt;frames_in);

    return in-&gt;read_status;

}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    in-&gt;frames_in -= buffer-&gt;frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;

    while (frames_wr &lt; frames) {
        size_t frames_rd = frames - frames_wr;
        if (in-&gt;resampler != NULL) {
            in-&gt;resampler-&gt;resample_from_provider(in-&gt;resampler,
                    (int16_t *)((char *)buffer +
                            frames_wr * audio_stream_frame_size(&amp;in-&gt;stream.common)),
                    &amp;frames_rd);
        } else {
            struct resampler_buffer buf = {
                    { raw : NULL, },
                    frame_count : frames_rd,
            };
            get_next_buffer(&amp;in-&gt;buf_provider, &amp;buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                           frames_wr * audio_stream_frame_size(&amp;in-&gt;stream.common),
                        buf.raw,
                        buf.frame_count * audio_stream_frame_size(&amp;in-&gt;stream.common));
                frames_rd = buf.frame_count;
            }
            release_buffer(&amp;in-&gt;buf_provider, &amp;buf);
        }
        /* in-&gt;read_status is updated by getNextBuffer() also called by
         * in-&gt;resampler-&gt;resample_from_provider() */
        if (in-&gt;read_status != 0)
            return in-&gt;read_status;

        frames_wr += frames_rd;
    }
    return frames_wr;
}

/* API functions */

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    return pcm_config_out.rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    return pcm_config_out.period_size *
               audio_stream_frame_size((struct audio_stream *)stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    return AUDIO_CHANNEL_OUT_STEREO;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    pthread_mutex_lock(&amp;out-&gt;dev-&gt;lock);
    pthread_mutex_lock(&amp;out-&gt;lock);
    do_out_standby(out);
    pthread_mutex_unlock(&amp;out-&gt;lock);
    pthread_mutex_unlock(&amp;out-&gt;dev-&gt;lock);

    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out-&gt;dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    unsigned int val;

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    pthread_mutex_lock(&amp;adev-&gt;lock);
    if (ret &gt;= 0) {
        val = atoi(value);
        if ((adev-&gt;out_device != val) &amp;&amp; (val != 0)) {
            /*
             * If SCO is turned on/off, we need to put audio into standby
             * because SCO uses a different PCM.
             */
            if ((val &amp; AUDIO_DEVICE_OUT_ALL_SCO) ^
                    (adev-&gt;out_device &amp; AUDIO_DEVICE_OUT_ALL_SCO)) {
                pthread_mutex_lock(&amp;out-&gt;lock);
                do_out_standby(out);
                pthread_mutex_unlock(&amp;out-&gt;lock);
            }

            adev-&gt;out_device = val;
            select_devices(adev);
        }
    }
    pthread_mutex_unlock(&amp;adev-&gt;lock);

    str_parms_destroy(parms);
    return ret;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    return strdup(&quot;&quot;);
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out-&gt;dev;
    size_t period_count;

    pthread_mutex_lock(&amp;adev-&gt;lock);

    if (adev-&gt;screen_off &amp;&amp; !adev-&gt;active_in &amp;&amp; !(adev-&gt;out_device &amp; AUDIO_DEVICE_OUT_ALL_SCO))
        period_count = OUT_LONG_PERIOD_COUNT;
    else
        period_count = OUT_SHORT_PERIOD_COUNT;

    pthread_mutex_unlock(&amp;adev-&gt;lock);

    return (pcm_config_out.period_size * period_count * 1000) / pcm_config_out.rate;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret = 0;
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out-&gt;dev;
    size_t frame_size = audio_stream_frame_size(&amp;out-&gt;stream.common);
    int16_t *in_buffer = (int16_t *)buffer;
    size_t in_frames = bytes / frame_size;
    size_t out_frames;
    int buffer_type;
    int kernel_frames;
    bool sco_on;

    /*
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the output stream mutex - e.g.
     * executing out_set_parameters() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&amp;adev-&gt;lock);
    pthread_mutex_lock(&amp;out-&gt;lock);
    if (out-&gt;standby) {
        ret = start_output_stream(out);
        if (ret != 0) {
            pthread_mutex_unlock(&amp;adev-&gt;lock);
            goto exit;
        }
        out-&gt;standby = false;
    }
    buffer_type = (adev-&gt;screen_off &amp;&amp; !adev-&gt;active_in) ?
            OUT_BUFFER_TYPE_LONG : OUT_BUFFER_TYPE_SHORT;
    sco_on = (adev-&gt;out_device &amp; AUDIO_DEVICE_OUT_ALL_SCO);
    pthread_mutex_unlock(&amp;adev-&gt;lock);

    /* detect changes in screen ON/OFF state and adapt buffer size
     * if needed. Do not change buffer size when routed to SCO device. */
    if (!sco_on &amp;&amp; (buffer_type != out-&gt;buffer_type)) {
        size_t period_count;

        if (buffer_type == OUT_BUFFER_TYPE_LONG)
            period_count = OUT_LONG_PERIOD_COUNT;
        else
            period_count = OUT_SHORT_PERIOD_COUNT;

        out-&gt;write_threshold = out-&gt;pcm_config-&gt;period_size * period_count;
        /* reset current threshold if exiting standby */
        if (out-&gt;buffer_type == OUT_BUFFER_TYPE_UNKNOWN)
            out-&gt;cur_write_threshold = out-&gt;write_threshold;
        out-&gt;buffer_type = buffer_type;
    }

    /* Reduce number of channels, if necessary */
    if (popcount(out_get_channels(&amp;stream-&gt;common)) &gt;
                 (int)out-&gt;pcm_config-&gt;channels) {
        unsigned int i;

        /* Discard right channel */
        for (i = 1; i &lt; in_frames; i++)
            in_buffer[i] = in_buffer[i * 2];

        /* The frame size is now half */
        frame_size /= 2;
    }

    /* Change sample rate, if necessary */
    if (out_get_sample_rate(&amp;stream-&gt;common) != out-&gt;pcm_config-&gt;rate) {
        out_frames = out-&gt;buffer_frames;
        out-&gt;resampler-&gt;resample_from_input(out-&gt;resampler,
                                            in_buffer, &amp;in_frames,
                                            out-&gt;buffer, &amp;out_frames);
        in_buffer = out-&gt;buffer;
    } else {
        out_frames = in_frames;
    }

    if (!sco_on) {
        int total_sleep_time_us = 0;
        size_t period_size = out-&gt;pcm_config-&gt;period_size;

        /* do not allow more than out-&gt;cur_write_threshold frames in kernel
         * pcm driver buffer */
        do {
            struct timespec time_stamp;
            if (pcm_get_htimestamp(out-&gt;pcm,
                                   (unsigned int *)&amp;kernel_frames,
                                   &amp;time_stamp) &lt; 0)
                break;
            kernel_frames = pcm_get_buffer_size(out-&gt;pcm) - kernel_frames;

            if (kernel_frames &gt; out-&gt;cur_write_threshold) {
                int sleep_time_us =
                    (int)(((int64_t)(kernel_frames - out-&gt;cur_write_threshold)
                                    * 1000000) / out-&gt;pcm_config-&gt;rate);
                if (sleep_time_us &lt; MIN_WRITE_SLEEP_US)
                    break;
                total_sleep_time_us += sleep_time_us;
                if (total_sleep_time_us &gt; MAX_WRITE_SLEEP_US) {
                    ALOGW(&quot;out_write() limiting sleep time %d to %d&quot;,
                          total_sleep_time_us, MAX_WRITE_SLEEP_US);
                    sleep_time_us = MAX_WRITE_SLEEP_US -
                                        (total_sleep_time_us - sleep_time_us);
                }
                usleep(sleep_time_us);
            }

        } while ((kernel_frames &gt; out-&gt;cur_write_threshold) &amp;&amp;
                (total_sleep_time_us &lt;= MAX_WRITE_SLEEP_US));

        /* do not allow abrupt changes on buffer size. Increasing/decreasing
         * the threshold by steps of 1/4th of the buffer size keeps the write
         * time within a reasonable range during transitions.
         * Also reset current threshold just above current filling status when
         * kernel buffer is really depleted to allow for smooth catching up with
         * target threshold.
         */
        if (out-&gt;cur_write_threshold &gt; out-&gt;write_threshold) {
            out-&gt;cur_write_threshold -= period_size / 4;
            if (out-&gt;cur_write_threshold &lt; out-&gt;write_threshold) {
                out-&gt;cur_write_threshold = out-&gt;write_threshold;
            }
        } else if (out-&gt;cur_write_threshold &lt; out-&gt;write_threshold) {
            out-&gt;cur_write_threshold += period_size / 4;
            if (out-&gt;cur_write_threshold &gt; out-&gt;write_threshold) {
                out-&gt;cur_write_threshold = out-&gt;write_threshold;
            }
        } else if ((kernel_frames &lt; out-&gt;write_threshold) &amp;&amp;
            ((out-&gt;write_threshold - kernel_frames) &gt;
                (int)(period_size * OUT_SHORT_PERIOD_COUNT))) {
            out-&gt;cur_write_threshold = (kernel_frames / period_size + 1) * period_size;
            out-&gt;cur_write_threshold += period_size / 4;
        }
    }

    ret = pcm_write(out-&gt;pcm, in_buffer, out_frames * frame_size);
    if (ret == -EPIPE) {
        /* In case of underrun, don&#39;t sleep since we want to catch up asap */
        pthread_mutex_unlock(&amp;out-&gt;lock);
        return ret;
    }

exit:
    pthread_mutex_unlock(&amp;out-&gt;lock);

    if (ret != 0) {
        usleep(bytes * 1000000 / audio_stream_frame_size(&amp;stream-&gt;common) /
               out_get_sample_rate(&amp;stream-&gt;common));
    }

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    return -EINVAL;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in-&gt;requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    size_t size;

    /*
     * take resampling into account and return the closest majoring
     * multiple of 16 frames, as audioflinger expects audio buffers to
     * be a multiple of 16 frames
     */
    size = (in-&gt;pcm_config-&gt;period_size * in_get_sample_rate(stream)) /
            in-&gt;pcm_config-&gt;rate;
    size = ((size + 15) / 16) * 16;

    return size * audio_stream_frame_size((struct audio_stream *)stream);
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    return AUDIO_CHANNEL_IN_MONO;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    pthread_mutex_lock(&amp;in-&gt;dev-&gt;lock);
    pthread_mutex_lock(&amp;in-&gt;lock);
    do_in_standby(in);
    pthread_mutex_unlock(&amp;in-&gt;lock);
    pthread_mutex_unlock(&amp;in-&gt;dev-&gt;lock);

    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in-&gt;dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    unsigned int val;

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    pthread_mutex_lock(&amp;adev-&gt;lock);
    if (ret &gt;= 0) {
        val = atoi(value) &amp; ~AUDIO_DEVICE_BIT_IN;
        if ((adev-&gt;in_device != val) &amp;&amp; (val != 0)) {
            /*
             * If SCO is turned on/off, we need to put audio into standby
             * because SCO uses a different PCM.
             */
            if ((val &amp; AUDIO_DEVICE_IN_ALL_SCO) ^
                    (adev-&gt;in_device &amp; AUDIO_DEVICE_IN_ALL_SCO)) {
                pthread_mutex_lock(&amp;in-&gt;lock);
                do_in_standby(in);
                pthread_mutex_unlock(&amp;in-&gt;lock);
            }

            adev-&gt;in_device = val;
            select_devices(adev);
        }
    }
    pthread_mutex_unlock(&amp;adev-&gt;lock);

    str_parms_destroy(parms);
    return ret;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    return strdup(&quot;&quot;);
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    int ret = 0;
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in-&gt;dev;
    size_t frames_rq = bytes / audio_stream_frame_size(&amp;stream-&gt;common);

    /*
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the input stream mutex - e.g.
     * executing in_set_parameters() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&amp;adev-&gt;lock);
    pthread_mutex_lock(&amp;in-&gt;lock);
    if (in-&gt;standby) {
        ret = start_input_stream(in);
        if (ret == 0)
            in-&gt;standby = 0;
    }
    pthread_mutex_unlock(&amp;adev-&gt;lock);

    if (ret &lt; 0)
        goto exit;

    /*if (in-&gt;num_preprocessors != 0) {
        ret = process_frames(in, buffer, frames_rq);
    } else */if (in-&gt;resampler != NULL) {
        ret = read_frames(in, buffer, frames_rq);
    } else if (in-&gt;pcm_config-&gt;channels == 2) {
        /*
         * If the PCM is stereo, capture twice as many frames and
         * discard the right channel.
         */
        unsigned int i;
        int16_t *in_buffer = (int16_t *)buffer;

        ret = pcm_read(in-&gt;pcm, in-&gt;buffer, bytes * 2);

        /* Discard right channel */
        for (i = 0; i &lt; frames_rq; i++)
            in_buffer[i] = in-&gt;buffer[i * 2];
    } else {
        ret = pcm_read(in-&gt;pcm, buffer, bytes);
    }

    if (ret &gt; 0)
        ret = 0;

    /*
     * Instead of writing zeroes here, we could trust the hardware
     * to always provide zeroes when muted.
     */
    if (ret == 0 &amp;&amp; adev-&gt;mic_mute)
        memset(buffer, 0, bytes);

exit:
    if (ret &lt; 0)
        usleep(bytes * 1000000 / audio_stream_frame_size(&amp;stream-&gt;common) /
               in_get_sample_rate(&amp;stream-&gt;common));

    pthread_mutex_unlock(&amp;in-&gt;lock);
    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream,
                               effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    return 0;
}


static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int ret;

    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (!out)
        return -ENOMEM;

    out-&gt;stream.common.get_sample_rate = out_get_sample_rate;
    out-&gt;stream.common.set_sample_rate = out_set_sample_rate;
    out-&gt;stream.common.get_buffer_size = out_get_buffer_size;
    out-&gt;stream.common.get_channels = out_get_channels;
    out-&gt;stream.common.get_format = out_get_format;
    out-&gt;stream.common.set_format = out_set_format;
    out-&gt;stream.common.standby = out_standby;
    out-&gt;stream.common.dump = out_dump;
    out-&gt;stream.common.set_parameters = out_set_parameters;
    out-&gt;stream.common.get_parameters = out_get_parameters;
    out-&gt;stream.common.add_audio_effect = out_add_audio_effect;
    out-&gt;stream.common.remove_audio_effect = out_remove_audio_effect;
    out-&gt;stream.get_latency = out_get_latency;
    out-&gt;stream.set_volume = out_set_volume;
    out-&gt;stream.write = out_write;
    out-&gt;stream.get_render_position = out_get_render_position;
    out-&gt;stream.get_next_write_timestamp = out_get_next_write_timestamp;

    out-&gt;dev = adev;

    config-&gt;format = out_get_format(&amp;out-&gt;stream.common);
    config-&gt;channel_mask = out_get_channels(&amp;out-&gt;stream.common);
    config-&gt;sample_rate = out_get_sample_rate(&amp;out-&gt;stream.common);

    out-&gt;standby = true;

    *stream_out = &amp;out-&gt;stream;
    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    out_standby(&amp;stream-&gt;common);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret;

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, &quot;orientation&quot;, value, sizeof(value));
    if (ret &gt;= 0) {
        int orientation;

        if (strcmp(value, &quot;landscape&quot;) == 0)
            orientation = ORIENTATION_LANDSCAPE;
        else if (strcmp(value, &quot;portrait&quot;) == 0)
            orientation = ORIENTATION_PORTRAIT;
        else if (strcmp(value, &quot;square&quot;) == 0)
            orientation = ORIENTATION_SQUARE;
        else
            orientation = ORIENTATION_UNDEFINED;

        pthread_mutex_lock(&amp;adev-&gt;lock);
        if (orientation != adev-&gt;orientation) {
            adev-&gt;orientation = orientation;
            /*
             * Orientation changes can occur with the input device
             * closed so we must call select_devices() here to set
             * up the mixer. This is because select_devices() will
             * not be called when the input device is opened if no
             * other input parameter is changed.
             */
            select_devices(adev);
        }
        pthread_mutex_unlock(&amp;adev-&gt;lock);
    }

    ret = str_parms_get_str(parms, &quot;screen_state&quot;, value, sizeof(value));
    if (ret &gt;= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev-&gt;screen_off = false;
        else
            adev-&gt;screen_off = true;
    }

    str_parms_destroy(parms);
    return ret;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    return strdup(&quot;&quot;);
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    adev-&gt;mic_mute = state;

    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev-&gt;mic_mute;

    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    size_t size;

    /*
     * take resampling into account and return the closest majoring
     * multiple of 16 frames, as audioflinger expects audio buffers to
     * be a multiple of 16 frames
     */
    size = (pcm_config_in.period_size * config-&gt;sample_rate) / pcm_config_in.rate;
    size = ((size + 15) / 16) * 16;

    return (size * popcount(config-&gt;channel_mask) *
                audio_bytes_per_sample(config-&gt;format));
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;
    int ret;

    *stream_in = NULL;

    /* Respond with a request for mono if a different format is given. */
    if (config-&gt;channel_mask != AUDIO_CHANNEL_IN_MONO) {
        config-&gt;channel_mask = AUDIO_CHANNEL_IN_MONO;
        return -EINVAL;
    }

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (!in)
        return -ENOMEM;

    in-&gt;stream.common.get_sample_rate = in_get_sample_rate;
    in-&gt;stream.common.set_sample_rate = in_set_sample_rate;
    in-&gt;stream.common.get_buffer_size = in_get_buffer_size;
    in-&gt;stream.common.get_channels = in_get_channels;
    in-&gt;stream.common.get_format = in_get_format;
    in-&gt;stream.common.set_format = in_set_format;
    in-&gt;stream.common.standby = in_standby;
    in-&gt;stream.common.dump = in_dump;
    in-&gt;stream.common.set_parameters = in_set_parameters;
    in-&gt;stream.common.get_parameters = in_get_parameters;
    in-&gt;stream.common.add_audio_effect = in_add_audio_effect;
    in-&gt;stream.common.remove_audio_effect = in_remove_audio_effect;
    in-&gt;stream.set_gain = in_set_gain;
    in-&gt;stream.read = in_read;
    in-&gt;stream.get_input_frames_lost = in_get_input_frames_lost;

    in-&gt;dev = adev;
    in-&gt;standby = true;
    in-&gt;requested_rate = config-&gt;sample_rate;
    in-&gt;pcm_config = &amp;pcm_config_in; /* default PCM config */

    *stream_in = &amp;in-&gt;stream;
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    in_standby(&amp;stream-&gt;common);
    free(stream);
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;

    audio_route_free(adev-&gt;ar);

    free(device);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct audio_device *adev;
    int ret;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct audio_device));
    if (!adev)
        return -ENOMEM;

    adev-&gt;hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev-&gt;hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev-&gt;hw_device.common.module = (struct hw_module_t *) module;
    adev-&gt;hw_device.common.close = adev_close;

    adev-&gt;hw_device.init_check = adev_init_check;
    adev-&gt;hw_device.set_voice_volume = adev_set_voice_volume;
    adev-&gt;hw_device.set_master_volume = adev_set_master_volume;
    adev-&gt;hw_device.set_mode = adev_set_mode;
    adev-&gt;hw_device.set_mic_mute = adev_set_mic_mute;
    adev-&gt;hw_device.get_mic_mute = adev_get_mic_mute;
    adev-&gt;hw_device.set_parameters = adev_set_parameters;
    adev-&gt;hw_device.get_parameters = adev_get_parameters;
    adev-&gt;hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev-&gt;hw_device.open_output_stream = adev_open_output_stream;
    adev-&gt;hw_device.close_output_stream = adev_close_output_stream;
    adev-&gt;hw_device.open_input_stream = adev_open_input_stream;
    adev-&gt;hw_device.close_input_stream = adev_close_input_stream;
    adev-&gt;hw_device.dump = adev_dump;

    adev-&gt;ar = audio_route_init();
    adev-&gt;orientation = ORIENTATION_UNDEFINED;
    adev-&gt;out_device = AUDIO_DEVICE_OUT_SPEAKER;
    adev-&gt;in_device = AUDIO_DEVICE_IN_BUILTIN_MIC &amp; ~AUDIO_DEVICE_BIT_IN;

    *device = &amp;adev-&gt;hw_device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = &quot;Grouper audio HW HAL&quot;,
        .author = &quot;The Android Open Source Project&quot;,
        .methods = &amp;hal_module_methods,
    },
};
</pre></body></html>
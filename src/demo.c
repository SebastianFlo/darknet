#include "network.h"
#include "detection_layer.h"
#include "region_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"
#include "image.h"
#include "demo.h"
#include "socket_server.h"
#include <sys/time.h>
#include "parson.h"

#define DEMO 1

#ifdef OPENCV

static char **demo_names;
static image **demo_alphabet;
static int demo_classes;

static float **probs;
static box *boxes;
static network *net;
static image buff [3];
static image buff_letter[3];
static int buff_index = 0;
static CvCapture * cap;
static IplImage  * ipl;
static float fps = 0;
static float demo_thresh = 0;
static float demo_hier = .5;
static int running = 0;

static int demo_frame = 3;
static int demo_detections = 0;
static float **predictions;
static int demo_index = 0;
static int demo_done = 0;
static float *avg;
double demo_time;

const char* get_current_time_in_ISO8601 () {
    static char current_time[sizeof "2011-10-08T07:07:0900Z"];
    time_t now;
    time(&now);
    strftime(current_time, sizeof(current_time), "%FT%TZ", localtime(&now));

    return current_time;
}

void *detect_in_thread(void *ptr)
{
    running = 1;
    float nms = .4;

    layer l = net->layers[net->n-1];
    float *X = buff_letter[(buff_index+2)%3].data;
    float *prediction = network_predict(net, X);

    memcpy(predictions[demo_index], prediction, l.outputs*sizeof(float));
    mean_arrays(predictions, demo_frame, l.outputs, avg);
    l.output = avg;
    if(l.type == DETECTION){
        get_detection_boxes(l, 1, 1, demo_thresh, probs, boxes, 0);
    } else if (l.type == REGION){
        get_region_boxes(l, buff[0].w, buff[0].h, net->w, net->h, demo_thresh, probs, boxes, 0, 0, 0, demo_hier, 1);
    } else {
        error("Last layer must produce detections\n");
    }
    if (nms > 0) do_nms_obj(boxes, probs, l.w*l.h*l.n, l.classes, nms);

    int i,j;

    // this is each frame
    char predictions[4096];
    char payload[4096];
    predictions[0] = 0;
    int count, last;
    count = 0;
    last = 0;

    // JSON_Value *root_value = json_value_init_object();
    // JSON_Object *root_object = json_value_get_object(root_value);
    // JSON_Array *payload = NULL;
    // static char *serialized_string = NULL;

    // json_object_set_string(root_object, "objectType", "receivedPayload");
    // json_object_set_string(root_object, "gwTimestamp", get_current_time_in_ISO8601());
    // json_object_set_string(root_object, "payloadType", "acknowledged");

    // json_object_set_value(root_object, "payload", json_value_init_array());

    // payload = json_object_get_array(root_object, "payload");

    // let's get all the i's where threshold is checked
    // for ( i = 0; i < demo_detections; i++ ) {
    //     for ( j = 0; j < demo_classes; j++ ) {
    //         if (probs[i][j] > demo_thresh){
    //             // JSON_Value *payload_value = json_value_init_object();
    //             // JSON_Object *payload_object = json_value_get_object(payload_value);

    //             // json_object_set_string(payload_object, "name", demo_names[j]);

    //             // json_array_append_value(payload, payload_object);


    //         //if not the first, add comma to the end
    //         if (count != 0) {
    //             count += snprintf(predictions + count, sizeof(predictions), ", { \"name\": \"%s\", \"prob\": %f }", demo_names[j], probs[i][j] );
    //         } else {
    //             count += snprintf(predictions + count, sizeof(predictions), "{ \"name\": \"%s\", \"prob\": %f }", demo_names[j], probs[i][j] );
    //             }

    //         }
    //     }
    // }

    //serialized_string = json_serialize_to_string(root_value);
    //printf("%s\n", serialized_string);
    //send_to_all(serialized_string);

    //json_free_serialized_string(serialized_string);
    //json_value_free(root_value);

    // let's get all the i's where threshold is checked
    for ( i = 0; i < demo_detections; i++ ) {
        for ( j = 0; j < demo_classes; j++ ) {
            if (probs[i][j] > demo_thresh){

                // if not the first, add comma to the end
                if (count != 0) {
                    count += snprintf(predictions + count, sizeof(predictions), ", { \"name\": \"%s\", \"prob\": %f }", demo_names[j], probs[i][j] );
                } else {
                    count += snprintf(predictions + count, sizeof(predictions), "{ \"name\": \"%s\", \"prob\": %f }", demo_names[j], probs[i][j] );
                }

            }
        }
    }

    // if no detections, be empty
    if (count == 0) {
        snprintf(payload, sizeof(predictions), "{ timestamp: %d, payload: [] }\n", (int)time(NULL));
    } else {
        snprintf(payload, sizeof(predictions), "{ timestamp: %d, payload: [ %s ]} \n", (int)time(NULL), predictions);
    }

    puts(payload);
    send_to_all(payload);

    demo_index = (demo_index + 1)%demo_frame;
    running = 0;
    return 0;
}

void *fetch_in_thread(void *ptr)
{
    int status = fill_image_from_stream(cap, buff[buff_index]);
    letterbox_image_into(buff[buff_index], net->w, net->h, buff_letter[buff_index]);
    if(status == 0) demo_done = 1;
    return 0;
}

void *display_in_thread(void *ptr)
{
    // show_image_cv(buff[(buff_index + 1)%3], "Demo", ipl);
    int c = cvWaitKey(1);
    if (c != -1) c = c%256;
    if (c == 27) {
        demo_done = 1;
        return 0;
    } else if (c == 82) {
        demo_thresh += .02;
    } else if (c == 84) {
        demo_thresh -= .02;
        if(demo_thresh <= .02) demo_thresh = .02;
    } else if (c == 83) {
        demo_hier += .02;
    } else if (c == 81) {
        demo_hier -= .02;
        if(demo_hier <= .0) demo_hier = .0;
    }
    return 0;
}

void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int delay, char *prefix, int avg_frames, float hier, int w, int h, int frames, int fullscreen)
{
    demo_frame = avg_frames;
    predictions = calloc(demo_frame, sizeof(float*));
    image **alphabet = load_alphabet();
    demo_names = names;
    demo_alphabet = alphabet;
    demo_classes = classes;
    demo_thresh = thresh;
    demo_hier = hier;
    printf("Demo\n");
    net = load_network(cfgfile, weightfile, 0);
    set_batch_network(net, 1);
    pthread_t detect_thread;
    pthread_t fetch_thread;

    srand(2222222);

    if(filename){
        printf("video file: %s\n", filename);
        cap = cvCaptureFromFile(filename);
    }else{
        cap = cvCaptureFromCAM(cam_index);

        if(w){
            cvSetCaptureProperty(cap, CV_CAP_PROP_FRAME_WIDTH, w);
        }
        if(h){
            cvSetCaptureProperty(cap, CV_CAP_PROP_FRAME_HEIGHT, h);
        }
        if(frames){
            cvSetCaptureProperty(cap, CV_CAP_PROP_FPS, frames);
        }
    }

    if(!cap) error("Couldn't connect to webcam.\n");

    layer l = net->layers[net->n-1];
    demo_detections = l.n*l.w*l.h;
    int j;

    avg = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < demo_frame; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));

    boxes = (box *)calloc(l.w*l.h*l.n, sizeof(box));
    probs = (float **)calloc(l.w*l.h*l.n, sizeof(float *));
    for(j = 0; j < l.w*l.h*l.n; ++j) probs[j] = (float *)calloc(l.classes+1, sizeof(float));

    buff[0] = get_image_from_stream(cap);
    buff[1] = copy_image(buff[0]);
    buff[2] = copy_image(buff[0]);
    buff_letter[0] = letterbox_image(buff[0], net->w, net->h);
    buff_letter[1] = letterbox_image(buff[0], net->w, net->h);
    buff_letter[2] = letterbox_image(buff[0], net->w, net->h);
    ipl = cvCreateImage(cvSize(buff[0].w,buff[0].h), IPL_DEPTH_8U, buff[0].c);

    int count = 0;
    int port = 6000;

    demo_time = what_time_is_it_now();

    pthread_t pth;
    SocketServerConf* conf = malloc(sizeof(SocketServerConf));
    conf->port = 6000;

    pthread_create(&pth, NULL, setup_socket_server, conf);

    while(!demo_done){
        buff_index = (buff_index + 1) %3;
        if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
        if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");
        if(!prefix){
            fps = 1./(what_time_is_it_now() - demo_time);
            demo_time = what_time_is_it_now();
            display_in_thread(0);
        }else{
            char name[256];
            sprintf(name, "%s_%08d", prefix, count);
            save_image(buff[(buff_index + 1)%3], name);
        }

        pthread_join(fetch_thread, 0);
        pthread_join(detect_thread, 0);
        ++count;
    }
}

#else
void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int delay, char *prefix, int avg, float hier, int w, int h, int frames, int fullscreen)
{
    fprintf(stderr, "Demo needs OpenCV for webcam images.\n");
}
#endif


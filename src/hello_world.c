#include <pebble.h>

static Window *window;
static TextLayer *text_layer;
short xmax = 0;
short ymax = 0;
short zmax = 0;
AppTimer *timer;


static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Double");
  //vibes_double_pulse();
  app_timer_cancel(timer); //Kill the callback timer recursion
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Long");
  //vibes_long_pulse();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Short");
  //vibes_short_pulse();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}




/*Normalize at 4000, the max value so that the smallest number is 0 instead of negative*/
static uint16_t mag_data(int16_t val) {
  int16_t newval = val + 4000;
  if (newval < 0){ //There was a huge pebble problem if this happens
    newval = 0;
  }
  return newval;
}

void timer_callback(void *data){
  vibes_long_pulse();
  timer = app_timer_register(1000, (AppTimerCallback) timer_callback, NULL); //So it perpetuates itself.
}

static void setup_timer(){
  timer = app_timer_register(1000, (AppTimerCallback) timer_callback, NULL);
}

static void accel_data_handler(AccelData *data, uint32_t num_samples){
  // Process 50 events every 5 seconds
  uint32_t ax = 0;
  uint32_t ay = 0;
  uint32_t az = 0;
  bool vibed = false;
  AccelData *d = data; //We are going to step through each sample with pointers!!!!
  for (uint32_t i = 0; i < num_samples; i++, d++){
    if(d->did_vibrate){
      vibed = true; //Can't use this set because getting a message may set up the scratch alarm
      break;
    }

    ax += mag_data(d->x);
    ay += mag_data(d->y);
    az += mag_data(d->z);

  }
  if(vibed){ return; } //Data is bad
  
  /*Averages*/
  ax /= num_samples;
  ay /= num_samples;
  az /= num_samples;
  
  /*Now find deviations from the average, if they are big we need to wake the user up*/
  AccelData *a = data;
  uint16_t biggest = 0; // Where we are going to show the biggest deviation and what we are going to check if is too big.
  for (uint32_t i = 0; i < num_samples; i++, a++){
    uint16_t x = mag_data(a->x);
    uint16_t y = mag_data(a->y);
    uint16_t z = mag_data(a->z);

    if ( x < ax){
      x = ax -x;
    } else {
      x -= ax;
    }
    if ( y < ay){
      y = ay -y;
    } else {
      y -= ay;
    }
    if ( z < az){
      z = az -z;
    } else {
      z -= az;
    }
    
    if (x > biggest) biggest = x;
    if (y > biggest) biggest = y;
    if (z > biggest) biggest = z;
  } 
  /*For now just log the data so we can decide what number we need, in production. Check if it is too big and then start vibrating*/
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Biggest deviation: %d", biggest);
  if(biggest > 1000){
    setup_timer();
  }
   
}




static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(text_layer, "Press a button");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
}

static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  accel_data_service_subscribe(10, accel_data_handler); 
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}


int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}

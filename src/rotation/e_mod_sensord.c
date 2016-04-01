#include "e_mod_main.h"
#include "e_mod_rotation.h"

#include <sensor_internal.h>

typedef struct _Pol_Sensord Pol_Sensord;

struct _Pol_Sensord
{
   sensor_t                        sensor;
   int                             handle;
   Eina_Bool                       started;
   int                             event;
   Ecore_Timer                    *retry_timer;
   int                             retry_count;
   Eina_Bool                       lock;
   Eina_Bool                       connected;
};

/* static global variables */
static Pol_Sensord _pol_sensor;

static Eina_Bool _sensor_connect(void);

static int
_ang_get(int event)
{
   int ang = -1;

   /* change CW (SensorFW) to CCW(EFL) */
   switch (event)
     {
      case AUTO_ROTATION_DEGREE_0:     ang = 0; break;
      case AUTO_ROTATION_DEGREE_90:    ang = 270; break;
      case AUTO_ROTATION_DEGREE_180:   ang = 180; break;
      case AUTO_ROTATION_DEGREE_270:   ang = 90; break;
      default:
         DBG("Unknown event %d", event);
        break;
     }

   if (!e_mod_pol_conf_rot_enable_get(ang))
     return -1;

   return ang;
}

static void
_sensor_rotation_changed_cb(sensor_t             sensor,
                            unsigned int         event_type,
                            sensor_data_t       *data,
                            void                *user_data)
{
    E_Zone *zone = e_zone_current_get();
    int event;
    int ang = 0;

    if (_pol_sensor.lock) return;
    if (!zone) return;
    if (event_type != AUTO_ROTATION_EVENT_CHANGE_STATE) return;

    event = (int)data->values[0];

    ang = _ang_get(event);

    DBG("ROT_EV event:%d angle:%d", event, ang);

    //e_zone_rotation_set(zone, ang);
    e_mod_rot_zone_set(zone, ang);

    _pol_sensor.event = event;
}

static Eina_Bool
_sensor_connect_retry_timeout(void *data)
{
   if (_pol_sensor.retry_timer)
     {
        ecore_timer_del(_pol_sensor.retry_timer);
        _pol_sensor.retry_timer = NULL;
     }
   _pol_sensor.retry_count++;
   DBG("retrying to connect _pol_sensor: count %d", _pol_sensor.retry_count);
   _sensor_connect();

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_sensor_connect(void)
{
   int h, r;

   if (_pol_sensor.connected) return EINA_TRUE;

   if (_pol_sensor.retry_timer)
     {
        ecore_timer_del(_pol_sensor.retry_timer);
        _pol_sensor.retry_timer = NULL;
     }

   _pol_sensor.sensor = sensord_get_sensor(AUTO_ROTATION_SENSOR);
   _pol_sensor.handle = -1;
   _pol_sensor.started = EINA_FALSE;

   h = sensord_connect(_pol_sensor.sensor);
   if (h < 0)
     {
        ERR("ERR! sensord_connect failed");
        goto error;
     }

   r = sensord_register_event(h, AUTO_ROTATION_EVENT_CHANGE_STATE,
                              SENSOR_INTERVAL_NORMAL, 0,
                              _sensor_rotation_changed_cb, NULL);
   if (r < 0)
     {
        ERR("ERR! sensord_register_event failed");
        sensord_disconnect(h);
        goto error;
     }

   r = sensord_start(h, 0);
   if (r < 0)
     {
        ERR("ERR! sensord_start failed");
        sensord_unregister_event(h, AUTO_ROTATION_EVENT_CHANGE_STATE);
        sensord_disconnect(h);
        goto error;
     }

   _pol_sensor.handle = h;
   _pol_sensor.started = EINA_TRUE;
   _pol_sensor.retry_count = 0;
   _pol_sensor.lock = EINA_FALSE;
   _pol_sensor.connected = EINA_TRUE;

   DBG("sensord_connect succeeded: handle %d", h);
   return EINA_TRUE;

error:
   if (_pol_sensor.retry_count <= 20)
     {
        _pol_sensor.retry_timer = ecore_timer_add(10.0f,
                                          _sensor_connect_retry_timeout,
                                          NULL);
     }
   return EINA_FALSE;
}

static Eina_Bool
_sensor_disconnect(void)
{
   int r;
   if (!_pol_sensor.connected) return EINA_TRUE;

   _pol_sensor.lock = EINA_FALSE;

   if (_pol_sensor.retry_timer)
     {
        ecore_timer_del(_pol_sensor.retry_timer);
        _pol_sensor.retry_timer = NULL;
     }

   _pol_sensor.retry_count = 0;

   if (_pol_sensor.handle < 0)
     {
        ERR("ERR! invalid handle %d", _pol_sensor.handle);
        goto error;
     }

   if (_pol_sensor.started)
     {
        r = sensord_unregister_event(_pol_sensor.handle,
                                AUTO_ROTATION_EVENT_CHANGE_STATE);
        if (r < 0)
          {
             ERR("ERR! sensord_unregister_event failed %d", r);
             goto error;
          }
        r = sensord_stop(_pol_sensor.handle);
        if (r < 0)
          {
             ERR("ERR! sensord_stop failed %d", r);
             goto error;
          }
        _pol_sensor.started = EINA_TRUE;
     }

   r = sensord_disconnect(_pol_sensor.handle);
   if (r < 0)
     {
        ERR("ERR! sensord_disconnect failed %d", r);
        goto error;
     }

   _pol_sensor.handle = -1;
   _pol_sensor.connected = EINA_FALSE;
   ERR("sensord_disconnect succeeded");
   return EINA_TRUE;
error:
   return EINA_FALSE;
}

EINTERN Eina_Bool
e_mod_sensord_init(void)
{
   _pol_sensor.connected = EINA_FALSE;
   _pol_sensor.retry_count = 0;
   return _sensor_connect();
}

EINTERN Eina_Bool
e_mod_sensord_deinit(void)
{
   _sensor_disconnect();
   return EINA_TRUE;
}

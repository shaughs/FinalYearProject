
package com.example.accelapp;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.cardview.widget.CardView;

import com.amazonaws.mobileconnectors.iot.AWSIotMqttClientStatusCallback;
import com.amazonaws.mobileconnectors.iot.AWSIotMqttNewMessageCallback;
import com.google.gson.Gson;

import java.io.UnsupportedEncodingException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;


public class MainActivity extends AppCompatActivity {

    /** MQTT connection */
    Connection mqttConnection;

    /** AWS related constants (endpoint, cognito, etc. */
    AwsConstants awsConstants;

    /** GUI items */
    ImageButton bAccelRefresh;
    ImageButton bRPMRefresh;
    ImageButton bMAFRefresh;
    ImageButton bLoadRefresh;
    ImageButton b02Refresh;
    TextView tvConnectionStatus;
    TextView tvAccelTimestamp;
    TextView tvRPMTimestamp;
    TextView tvMAFTimestamp;
    TextView tvLoadTimestamp;
    TextView tv02Timestamp;
    TextView tvAccelValues;
    TextView tvRPMValues;
    TextView tvMAFValues;
    TextView tvLoadValues;
    TextView tv02Values;
    ProgressBar progressBarConnection;
    ProgressBar progressBarAccel;
    ProgressBar progressBarMAF;
    ProgressBar progressBarRPM;
    ProgressBar progressBarLoad;
    ProgressBar progressBar02;
    CardView cardViewAccel;
    CardView cardViewMAF;
    CardView cardViewRPM;
    CardView cardViewLoad;
    CardView cardView02;

    /** variables */
    Handler timeoutHandler;
    Runnable displayTimeoutToast;
    Runnable mqttTryToConnect;
    boolean doubleBackToExit;
    boolean mqttConnected;
    com.example.accelapp.IPreferencesListener preferencesListener;
    AWSIotMqttClientStatusCallback mqttClientStatusCallback;

    /** Colours */
    int redColour;
    int greenColour;
    int blueColour;
    int blackColour;

    /** Preferences file dialog ID */
    private static final int FILE_DIALOG_ID = 5;

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // GUI items instances
        bAccelRefresh = (ImageButton) findViewById(R.id.bAccelRefresh);
        bRPMRefresh = (ImageButton) findViewById(R.id.bRPMRefresh);
        bMAFRefresh = (ImageButton) findViewById(R.id.bMAFRefresh);
        bLoadRefresh = (ImageButton) findViewById(R.id.bLoadRefresh);
        b02Refresh = (ImageButton) findViewById(R.id.b02Refresh);
        tvConnectionStatus = (TextView) findViewById(R.id.tvConnectionStatus);
        tvAccelTimestamp = (TextView) findViewById(R.id.tvAccelTimestamp);
        tvRPMTimestamp = (TextView) findViewById(R.id.tvRPMTimestamp);
        tvMAFTimestamp = (TextView) findViewById(R.id.tvMAFTimestamp);
        tvLoadTimestamp = (TextView) findViewById(R.id.tvLoadTimestamp);
        tv02Timestamp = (TextView) findViewById(R.id.tv02Timestamp);
        tvAccelValues = (TextView) findViewById(R.id.tvAccelValues);
        tvRPMValues = (TextView) findViewById(R.id.tvRPMValues);
        tvMAFValues = (TextView) findViewById(R.id.tvMAFValues);
        tvLoadValues = (TextView) findViewById(R.id.tvLoadValues);
        tv02Values = (TextView) findViewById(R.id.tv02Values);
        progressBarConnection = (ProgressBar) findViewById(R.id.progressBarConnection);
        progressBarAccel = (ProgressBar) findViewById(R.id.progressBarAccel);
        progressBarRPM = (ProgressBar) findViewById(R.id.progressBarRPM);
        progressBarMAF = (ProgressBar) findViewById(R.id.progressBarMAF);
        progressBarLoad = (ProgressBar) findViewById(R.id.progressBarLoad);
        progressBar02 = (ProgressBar) findViewById(R.id.progressBar02);
        cardViewAccel = (CardView) findViewById(R.id.card_view_accel);
        cardViewRPM = (CardView) findViewById(R.id.card_view_RPM);
        cardViewMAF = (CardView) findViewById(R.id.card_view_MAF);
        cardViewLoad = (CardView) findViewById(R.id.card_view_Load);
        cardView02 = (CardView) findViewById(R.id.card_view_02);

        // default variable values
        mqttConnected = false;
        redColour = getResources().getColor(android.R.color.holo_red_dark);
        greenColour = getResources().getColor(android.R.color.holo_green_dark);
        blueColour = getResources().getColor(android.R.color.holo_blue_dark);
        blackColour = getResources().getColor(android.R.color.black);
        timeoutHandler = new Handler();
        final AppCompatActivity activity = this;
        displayTimeoutToast = new Runnable() {
            public void run() {
                Toast.makeText(
                    getApplicationContext(),
                    "Device is not responding.",
                    Toast.LENGTH_LONG
                ).show();

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        progressBarAccel.setVisibility(View.INVISIBLE);
                        progressBarRPM.setVisibility(View.INVISIBLE);
                        progressBarMAF.setVisibility(View.INVISIBLE);
                        progressBarLoad.setVisibility(View.INVISIBLE);
                        progressBar02.setVisibility(View.INVISIBLE);
                    }
                });

                enableClickableGUIItems(mqttConnected);
            }
        };
        mqttTryToConnect = new Runnable() {
            public void run() {
                Log.d(Connection.LOG_TAG, "Trying to connect to MQTT");

                if (!isNetworkAvailable()) {
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            tvConnectionStatus.setText("Network not available");
                            progressBarConnection.setVisibility(View.INVISIBLE);
                        }
                    });

                    // run timer again
                    timeoutHandler.postDelayed(this, AwsConstants.CONNECT_TIMEOUT);
                } else {
                    // network available, try to connect
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            tvConnectionStatus.setText("Connecting...");
                            progressBarConnection.setVisibility(View.VISIBLE);
                        }
                    });

                    final boolean isConnected = mqttConnection.connect(mqttClientStatusCallback);

                    if (isConnected) {
                        // cancel timeout
                        timeoutHandler.removeCallbacks(this);
                    } else {
                        new AlertDialog.Builder(activity).setTitle("Could not connect to AWS IoT")
                                .setMessage("Error during creation of AWS IoT client.\nPlease check *.properties file.")
                                .setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog, int which) {
                                        // open file choose dialog
                                        openFileDialog();
                                    }
                                })
                                .show();
                    }
                }
            }
        };

        // by default disable GUI items
        enableClickableGUIItems(false);

        // Update LED value in device shadow by setting which led was turned on as shadow's desired state.
        CompoundButton.OnCheckedChangeListener rgbLedSwitchListener = new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                AwsShadow shadow = new AwsShadow();

                // reported and metadata must not be sent along with desired shadow state
                shadow.state.desired.accelUpdate = null;
                shadow.state.desired.rpmUpdate = null;                                                  //RPM
                shadow.state.desired.mafUpdate = null;                                                  //MAF
                shadow.state.desired.loadUpdate = null;                                                 //Engine Load
                shadow.state.desired.osUpdate = null;                                                 //Engine Load
                shadow.state.reported = null;
                shadow.metadata = null;

                // disable buttons
                enableClickableGUIItems(false);

                // create message json in format of device shadow
                final String message = new Gson().toJson(shadow, AwsShadow.class);

                // publish message
                mqttConnection.publish(awsConstants.SHADOW_UPDATE, message);

                timeoutHandler.postDelayed(displayTimeoutToast, AwsConstants.TIMEOUT);
            }
        };

        bAccelRefresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AwsShadow shadow = new AwsShadow();
                shadow.state.desired.accelUpdate = 1;

                // reported and metadata must not be sent along with desired shadow state
                shadow.state.reported = null;
                shadow.metadata = null;

                // disable buttons
                enableClickableGUIItems(false);

                // create message json in format of device shadow
                final String message = new Gson().toJson(shadow, AwsShadow.class);

                // publish message
                mqttConnection.publish(awsConstants.SHADOW_UPDATE, message);

                // show progress bar and wait for certain time
                // if no message from device has been received, display toast message
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        progressBarAccel.setVisibility(View.VISIBLE);
                    }
                });
                timeoutHandler.postDelayed(displayTimeoutToast, AwsConstants.TIMEOUT);

                Log.d(Connection.LOG_TAG, "Accelerometer update request was send.");
            }
        });

        bRPMRefresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AwsShadow shadow = new AwsShadow();
                shadow.state.desired.rpmUpdate = 1;

                // reported and metadata must not be sent along with desired shadow state
                shadow.state.reported = null;
                shadow.metadata = null;

                // disable buttons
                enableClickableGUIItems(false);

                // create message json in format of device shadow
                final String message = new Gson().toJson(shadow, AwsShadow.class);

                // publish message
                mqttConnection.publish(awsConstants.SHADOW_UPDATE, message);

                // show progress bar and wait for certain time
                // if no message from device has been received, display toast message
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        progressBarRPM.setVisibility(View.VISIBLE);
                    }
                });
                timeoutHandler.postDelayed(displayTimeoutToast, AwsConstants.TIMEOUT);

                Log.d(Connection.LOG_TAG, "RPM update request was send.");
            }
        });

        bMAFRefresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AwsShadow shadow = new AwsShadow();
                shadow.state.desired.mafUpdate = 1;

                // reported and metadata must not be sent along with desired shadow state
                shadow.state.reported = null;
                shadow.metadata = null;

                // disable buttons
                enableClickableGUIItems(false);

                // create message json in format of device shadow
                final String message = new Gson().toJson(shadow, AwsShadow.class);

                // publish message
                mqttConnection.publish(awsConstants.SHADOW_UPDATE, message);

                // show progress bar and wait for certain time
                // if no message from device has been received, display toast message
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        progressBarMAF.setVisibility(View.VISIBLE);
                    }
                });
                timeoutHandler.postDelayed(displayTimeoutToast, AwsConstants.TIMEOUT);

                Log.d(Connection.LOG_TAG, "MAF update request was send.");
            }
        });

        bLoadRefresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AwsShadow shadow = new AwsShadow();
                shadow.state.desired.loadUpdate = 1;

                // reported and metadata must not be sent along with desired shadow state
                shadow.state.reported = null;
                shadow.metadata = null;

                // disable buttons
                enableClickableGUIItems(false);

                // create message json in format of device shadow
                final String message = new Gson().toJson(shadow, AwsShadow.class);

                // publish message
                mqttConnection.publish(awsConstants.SHADOW_UPDATE, message);

                // show progress bar and wait for certain time
                // if no message from device has been received, display toast message
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        progressBarLoad.setVisibility(View.VISIBLE);
                    }
                });
                timeoutHandler.postDelayed(displayTimeoutToast, AwsConstants.TIMEOUT);

                Log.d(Connection.LOG_TAG, "Engine Load update request was send.");
            }
        });

        b02Refresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AwsShadow shadow = new AwsShadow();
                shadow.state.desired.osUpdate = 1;

                // reported and metadata must not be sent along with desired shadow state
                shadow.state.reported = null;
                shadow.metadata = null;

                // disable buttons
                enableClickableGUIItems(false);

                // create message json in format of device shadow
                final String message = new Gson().toJson(shadow, AwsShadow.class);

                // publish message
                mqttConnection.publish(awsConstants.SHADOW_UPDATE, message);

                // show progress bar and wait for certain time
                // if no message from device has been received, display toast message
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        progressBar02.setVisibility(View.VISIBLE);
                    }
                });
                timeoutHandler.postDelayed(displayTimeoutToast, AwsConstants.TIMEOUT);

                Log.d(Connection.LOG_TAG, "Oxygen Sensor update request was send.");
            }
        });


        // create preferences listener
        // on success, establish MQTT connection
        // on error, try to load preferences from file
        preferencesListener = new com.example.accelapp.IPreferencesListener() {
            @Override
            public void onPreferencesLoadSuccess() {
                Toast.makeText(
                        getApplicationContext(),
                        "Preferences loaded successfully.",
                        Toast.LENGTH_LONG
                ).show();

                // create MQTT client status callback
                mqttClientStatusCallback = new AWSIotMqttClientStatusCallback() {
                    @Override
                    public void onStatusChanged(final AWSIotMqttClientStatus status,
                                                final Throwable throwable) {
                        Log.d(Connection.LOG_TAG, "MQTT connection status: " + String.valueOf(status));

                        // refreshShadowUrls GUI
                        mqttConnected = (status == AWSIotMqttClientStatus.Connected);
                        enableClickableGUIItems(mqttConnected);

                        // display connection status on dashboard
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                switch (status) {
                                    case Connecting:
                                        tvConnectionStatus.setText("Connecting...");
                                        progressBarConnection.setVisibility(View.VISIBLE);
                                        break;

                                    case Connected:
                                        // MQTT connection is ready, can do publish and subscribe
                                        tvConnectionStatus.setText(String.format("Connected to \"%s\" thing", awsConstants.getThingName()));
                                        progressBarConnection.setVisibility(View.INVISIBLE);
                                        break;

                                    case Reconnecting:
                                        if (throwable != null) {
                                            Log.e(Connection.LOG_TAG, "Connection error.", throwable);
                                        }
                                        if (isNetworkAvailable()) {
                                            tvConnectionStatus.setText("Reconnecting");
                                            progressBarConnection.setVisibility(View.VISIBLE);
                                        } else {
                                            tvConnectionStatus.setText("Network not available");
                                            progressBarConnection.setVisibility(View.INVISIBLE);
                                        }
                                        break;

                                    case ConnectionLost:
                                        if (throwable != null) {
                                            Log.e(Connection.LOG_TAG, "Connection error.", throwable);
                                            throwable.printStackTrace();
                                        }
                                        tvConnectionStatus.setText("Connection error");
                                        progressBarConnection.setVisibility(View.INVISIBLE);
                                        break;

                                    default:
                                        tvConnectionStatus.setText("Disconnected");
                                        progressBarConnection.setVisibility(View.INVISIBLE);
                                        break;
                                }
                            }
                        });

                        // when MQTT manager is connected to AWS server
                        // subscribe for accepted updates from AWS remote control example shadow
                        // and download shadow after successful connection
                        if (status == AWSIotMqttClientStatus.Connected) {
                            // subscription callback for updating GUI values
                            final AWSIotMqttNewMessageCallback subscriptionCallback = new AWSIotMqttNewMessageCallback() {
                                @Override
                                public void onMessageArrived(final String topic, final byte[] data) {
                                    try {
                                        String message = new String(data, "UTF-8");

                                        // parse message from JSON into class
                                        Gson gson = new Gson();
                                        final AwsShadow shadow = gson.fromJson(message, AwsShadow.class);

                                        Log.d(Connection.LOG_TAG, String.format("Received message \"%s\" from topic \"%s\"", message, topic));

                                        if (shadow != null && shadow.state.reported != null) {
                                            // enable GUI items
                                            enableClickableGUIItems(true);

                                            // accelerometer
                                            updateAccelValuesAfterShadowUpdate(shadow);

                                            // remove timeout callback
                                            timeoutHandler.removeCallbacks(displayTimeoutToast);

                                            // last known shadow state received
                                            if (topic.equals(awsConstants.SHADOW_GET_ACCEPTED)) {
                                                runOnUiThread(new Runnable() {
                                                    @Override
                                                    public void run() {
                                                        Toast.makeText(
                                                                getApplicationContext(),
                                                                "Last known device shadow state has been received.",
                                                                Toast.LENGTH_LONG
                                                        ).show();

                                                        // hide if no data has been received
                                                        cardViewAccel.setVisibility(shadow.state.reported.accel == null ? View.GONE : View.VISIBLE);
                                                    }
                                                });
                                            }

                                            updateRPMValuesAfterShadowUpdate(shadow);

                                            // remove timeout callback
                                            timeoutHandler.removeCallbacks(displayTimeoutToast);

                                            // last known shadow state received
                                            if (topic.equals(awsConstants.SHADOW_GET_ACCEPTED)) {
                                                runOnUiThread(new Runnable() {
                                                    @Override
                                                    public void run() {
                                                        Toast.makeText(
                                                                getApplicationContext(),
                                                                "Last known device shadow state has been received.",
                                                                Toast.LENGTH_LONG
                                                        ).show();

                                                        // hide if no data has been received
                                                        cardViewRPM.setVisibility(shadow.state.reported.rpm == null ? View.GONE : View.VISIBLE);
                                                    }
                                                });
                                            }

                                            updateMAFValuesAfterShadowUpdate(shadow);

                                            // remove timeout callback
                                            timeoutHandler.removeCallbacks(displayTimeoutToast);

                                            // last known shadow state received
                                            if (topic.equals(awsConstants.SHADOW_GET_ACCEPTED)) {
                                                runOnUiThread(new Runnable() {
                                                    @Override
                                                    public void run() {
                                                        Toast.makeText(
                                                                getApplicationContext(),
                                                                "Last known device shadow state has been received.",
                                                                Toast.LENGTH_LONG
                                                        ).show();

                                                        // hide if no data has been received
                                                        cardViewMAF.setVisibility(shadow.state.reported.maf == null ? View.GONE : View.VISIBLE);
                                                    }
                                                });
                                            }

                                            updateLOADValuesAfterShadowUpdate(shadow);

                                            // remove timeout callback
                                            timeoutHandler.removeCallbacks(displayTimeoutToast);

                                            // last known shadow state received
                                            if (topic.equals(awsConstants.SHADOW_GET_ACCEPTED)) {
                                                runOnUiThread(new Runnable() {
                                                    @Override
                                                    public void run() {
                                                        Toast.makeText(
                                                                getApplicationContext(),
                                                                "Last known device shadow state has been received.",
                                                                Toast.LENGTH_LONG
                                                        ).show();

                                                        // hide if no data has been received
                                                        cardViewLoad.setVisibility(shadow.state.reported.maf == null ? View.GONE : View.VISIBLE);
                                                    }
                                                });
                                            }

                                            updateOSValuesAfterShadowUpdate(shadow);

                                            // remove timeout callback
                                            timeoutHandler.removeCallbacks(displayTimeoutToast);

                                            // last known shadow state received
                                            if (topic.equals(awsConstants.SHADOW_GET_ACCEPTED)) {
                                                runOnUiThread(new Runnable() {
                                                    @Override
                                                    public void run() {
                                                        Toast.makeText(
                                                                getApplicationContext(),
                                                                "Last known device shadow state has been received.",
                                                                Toast.LENGTH_LONG
                                                        ).show();

                                                        // hide if no data has been received
                                                        cardView02.setVisibility(shadow.state.reported.maf == null ? View.GONE : View.VISIBLE);
                                                    }
                                                });
                                            }
                                        } else {
                                            runOnUiThread(new Runnable() {
                                                @Override
                                                public void run() {
                                                    if (topic.equals(awsConstants.SHADOW_GET_ACCEPTED)) {
                                                        Toast.makeText(
                                                                getApplicationContext(),
                                                                "Device shadow has no reported state.",
                                                                Toast.LENGTH_LONG
                                                        ).show();
                                                    }
                                                }
                                            });
                                        }

                                    } catch (UnsupportedEncodingException e) {
                                        Log.e(Connection.LOG_TAG, "Message encoding error.", e);
                                    } catch (Exception e) {
                                        Log.e(Connection.LOG_TAG, "Could not parse message JSON.", e);
                                    }
                                }
                            };

                            // subscribe for all accepted changes (used for accelerometer values)
                            mqttConnection.subscribe(awsConstants.SHADOW_UPDATE_ACCEPTED, subscriptionCallback);

                            // get device shadow after successful connection
                            mqttConnection.subscribe(awsConstants.SHADOW_GET_ACCEPTED, subscriptionCallback);

                            // wait one second for subscribe to be done before sending GET request
                            timeoutHandler.postDelayed(new Runnable() {
                                @Override
                                public void run() {
                                    // make publish
                                    mqttConnection.publish(awsConstants.SHADOW_GET, AwsConstants.EMPTY_MESSAGE);

                                    // wait for certain time for device response
                                    timeoutHandler.postDelayed(displayTimeoutToast, AwsConstants.TIMEOUT);
                                }
                            }, 1000);
                        }
                    }
                };

                // Create new MQTT connection instance
                mqttConnection = new Connection(getApplicationContext(), awsConstants);

                // try to establish MQTT connection
                timeoutHandler.postDelayed(mqttTryToConnect, 1);
            }

            @Override
            public void onPreferencesLoadError(String errMsg) {
                new AlertDialog.Builder(activity).setTitle("Could not load preferences")
                    .setMessage("Could not load properties file with AWS IoT preferences.\nReason: " +
                            errMsg + "\nPlease select correct *.properties file.")
                    .setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int which) {
                            // open file choose dialog
                            openFileDialog();
                        }
                    })
                    .show();
            }
        };

        // check saved preferences
        Context context = getApplicationContext();
        awsConstants = new AwsConstants(context, getString(R.string.preference_file_key));

        if (awsConstants.checkPreferencesAndValidate()) {
            // preferences are loaded, connect to MQTT
            awsConstants.refreshShadowUrls();
            preferencesListener.onPreferencesLoadSuccess();
        } else {
            // preferences not loaded yet, display dialog for loading preferences from file
            new AlertDialog.Builder(this).setTitle("Load preferences")
                .setMessage("Please locate properties file with AWS IoT preferences (e.g. Customer specific endpoint, Cognito pool ID, etc.).")
                .setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        // open file choose dialog
                        openFileDialog();
                    }
                })
                .show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent intent) {
        super.onActivityResult(requestCode, resultCode, intent);

        // file dialog result
        if (requestCode == FILE_DIALOG_ID && resultCode == RESULT_OK && intent != null) {
            // load preferences from selected file
            awsConstants.loadPreferencesFromFile(intent.getData(), preferencesListener);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (mqttConnection != null && !mqttConnection.disconnect()) {
            Log.i(Connection.LOG_TAG, "Could not disconnect from AWS IoT.");
        }
    }

    /**
     * Enable {or disable} clickable GUI items (e.g. switches, buttons, atc.)
     * @param enable
     */
    private void enableClickableGUIItems(final boolean enable) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                bAccelRefresh.setEnabled(enable);
                bRPMRefresh.setEnabled(enable);
                bMAFRefresh.setEnabled(enable);
                bLoadRefresh.setEnabled(enable);
                b02Refresh.setEnabled(enable);
            }
        });
    }

    /**
     * Update accelerometer values after receiving shadow update from subscription.
     * @param shadow AWS shadow
     */
    private void updateAccelValuesAfterShadowUpdate(final AwsShadow shadow) {
        if (shadow.state.reported != null){
            if (shadow.state.reported.accel != null) {
                final AwsShadow.State.Reported.Accel accel = shadow.state.reported.accel;

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        cardViewAccel.setVisibility(View.VISIBLE);

                        // update accelerometer values
                        tvAccelValues.setText(String.format("x: %d  y: %d  z: %d", accel.x, accel.y, accel.z));

                        // update timestamp
                        tvAccelTimestamp.setText(formatUnixTimeStamp(shadow.metadata.reported.accel.x.timestamp));

                        // hide progress bar
                        progressBarAccel.setVisibility(View.INVISIBLE);
                    }
                });
            }
        }
    }

    /**
     * Update rpm values after receiving shadow update from subscription.
     * @param shadow AWS shadow
     */
    private void updateRPMValuesAfterShadowUpdate(final AwsShadow shadow) {
        if (shadow.state.reported != null){
            if (shadow.state.reported.rpm != null) {
                final AwsShadow.State.Reported.RPM rpm = shadow.state.reported.rpm;

                runOnUiThread(new Runnable() {
                    @SuppressLint("DefaultLocale")
                    @Override
                    public void run() {
                        cardViewRPM.setVisibility(View.VISIBLE);

                        // update rpm values
                        tvRPMValues.setText(String.format("RPM: %d", rpm.rpm));

                        // update timestamp
                        tvRPMTimestamp.setText(formatUnixTimeStamp(shadow.metadata.reported.rpm.rpm.timestamp));

                        // hide progress bar
                        progressBarRPM.setVisibility(View.INVISIBLE);
                    }
                });
            }
        }
    }

    /**
     * Update maf values after receiving shadow update from subscription.
     * @param shadow AWS shadow
     */
    private void updateMAFValuesAfterShadowUpdate(final AwsShadow shadow) {
        if (shadow.state.reported != null){
            if (shadow.state.reported.maf != null) {
                final AwsShadow.State.Reported.MAF maf = shadow.state.reported.maf;

                runOnUiThread(new Runnable() {
                    @SuppressLint("DefaultLocale")
                    @Override
                    public void run() {
                        cardViewMAF.setVisibility(View.VISIBLE);

                        // update maf values
                        tvMAFValues.setText(String.format("MAF: %d", maf.maf));

                        // update timestamp
                        tvMAFTimestamp.setText(formatUnixTimeStamp(shadow.metadata.reported.maf.maf.timestamp));

                        // hide progress bar
                        progressBarMAF.setVisibility(View.INVISIBLE);
                    }
                });
            }
        }
    }

    /**
     * Update engine load values after receiving shadow update from subscription.
     * @param shadow AWS shadow
     */
    private void updateLOADValuesAfterShadowUpdate(final AwsShadow shadow) {
        if (shadow.state.reported != null){
            if (shadow.state.reported.load != null) {
                final AwsShadow.State.Reported.LOAD load = shadow.state.reported.load;

                runOnUiThread(new Runnable() {
                    @SuppressLint("DefaultLocale")
                    @Override
                    public void run() {
                        cardViewLoad.setVisibility(View.VISIBLE);

                        // update engine load values
                        tvLoadValues.setText(String.format("Engine Load: %d", load.load));

                        // update timestamp
                        tvLoadTimestamp.setText(formatUnixTimeStamp(shadow.metadata.reported.load.load.timestamp));

                        // hide progress bar
                        progressBarLoad.setVisibility(View.INVISIBLE);
                    }
                });
            }
        }
    }

    /**
     * Update oxygen sensor values after receiving shadow update from subscription.
     * @param shadow AWS shadow
     */
    private void updateOSValuesAfterShadowUpdate(final AwsShadow shadow) {
        if (shadow.state.reported != null){
            if (shadow.state.reported.os != null) {
                final AwsShadow.State.Reported.OS os = shadow.state.reported.os;

                runOnUiThread(new Runnable() {
                    @SuppressLint("DefaultLocale")
                    @Override
                    public void run() {
                        cardView02.setVisibility(View.VISIBLE);

                        // update oxygen sensor values
                        tv02Values.setText(String.format("02 Value: %d", os.os));

                        // update timestamp
                        tv02Timestamp.setText(formatUnixTimeStamp(shadow.metadata.reported.os.os.timestamp));

                        // hide progress bar
                        progressBar02.setVisibility(View.INVISIBLE);
                    }
                });
            }
        }
    }

    /**
     * Format Unix timestamp to human readable format
     */
    private String formatUnixTimeStamp(Long timestamp) {
        DateFormat sdf = new SimpleDateFormat("dd.MM.yyyy hh:mm:ss a", Locale.US);
        sdf.setTimeZone(TimeZone.getDefault());
        return sdf.format(new Date(timestamp * 1000L));
    }

    /**
     * Open file dialog
     */
    private void openFileDialog() {
        Intent intent = new Intent()
            .setType("*/*")
            .setAction(Intent.ACTION_GET_CONTENT);
        startActivityForResult(Intent.createChooser(intent, "Select a preferences file"), FILE_DIALOG_ID);
    }

    /**
     * Check if any kind of network (WiFi or mobile) connection is available
     */
    private boolean isNetworkAvailable() {
        final ConnectivityManager connectivity = (ConnectivityManager) getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivity != null) {
            final NetworkInfo[] infos = connectivity.getAllNetworkInfo();
            if (infos != null) {
                for (NetworkInfo info : infos) {
                    if (info.getState() == NetworkInfo.State.CONNECTED) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    @Override
    public void onBackPressed() {
        if (doubleBackToExit) {
            super.onBackPressed();
            return;
        }

        this.doubleBackToExit = true;
        Toast.makeText(
                getApplicationContext(),
                "Please press back once more to exit",
                Toast.LENGTH_SHORT
        ).show();

        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                doubleBackToExit = false;
            }
        }, 2000);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
       // getMenuInflater().inflate(R.menu.main, menu); //********************************************************************************************************************************************
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        switch (id) {
            case R.id.menuItemLoadPreferences:
                // open file dialog for loading preferences
                openFileDialog();
                return true;
        }

        return super.onOptionsItemSelected(item);
    }
}

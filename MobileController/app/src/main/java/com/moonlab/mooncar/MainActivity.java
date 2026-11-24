package com.moonlab.mooncar;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends Activity {
    private static final int BACKGROUND = 0xff0d1016;
    private static final int PANEL = 0xff171b24;
    private static final int BORDER = 0xff303644;
    private static final int TEXT = 0xfff1f3f5;
    private static final int MUTED = 0xffa5aebe;
    private static final int ACCENT = 0xffdafa53;
    private static final int RED = 0xffff8d88;
    private static final int PERMISSION_REQUEST = 1;
    private static final String[] SENSOR_LABELS = {
            "전방 · PB0/PB1", "좌측 · PC4/PC5", "우측 · PC6/PC7"
    };

    private final Handler main = new Handler(Looper.getMainLooper());
    private final BluetoothLink link = new BluetoothLink(this::onLinkState, this::onTelemetry);
    private final DriveController drive = new DriveController(link::send);
    private final Runnable telemetryDeadline = () -> clearTelemetry("센서 정보 수신 없음");
    private final Runnable pairingTimeout = () -> cancelPairing("페어링 시간이 초과됐습니다");
    private final Runnable cameraRefresh = new Runnable() {
        @Override public void run() {
            updateCamera();
            main.postDelayed(this, 40);
        }
    };
    private final BroadcastReceiver pairingReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            BluetoothDevice device = BluetoothPicker.deviceFrom(intent);
            if (device == null || !device.equals(pendingPairing)) return;
            int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.ERROR);
            if (state == BluetoothDevice.BOND_BONDED) {
                finishPairing();
            } else if (state == BluetoothDevice.BOND_NONE) {
                cancelPairing("페어링이 완료되지 않았습니다");
            }
        }
    };
    private final Runnable repeat = new Runnable() {
        private int sensorTicks;

        @Override public void run() {
            onControlTick();
            drive.repeat(sensorTicks == 0);
            sensorTicks = (sensorTicks + 1) % 3;
            main.postDelayed(this, 120);
        }
    };

    private BluetoothLink.State linkState = BluetoothLink.State.DISCONNECTED;
    private BluetoothPicker picker;
    private BluetoothDevice pendingPairing;
    private String pairingName;
    private boolean pairingReceiverRegistered;
    private LinearLayout root;
    private TextView connectionStatus;
    private Button connectionButton;
    private Button cameraButton;
    private CameraStream camera;
    private CameraStream.Frame displayedFrame;
    private FrameLayout cameraPanel;
    private LinearLayout modeCard;
    private View modeTitle;
    private LinearLayout modeButtons;
    private ImageView cameraImage;
    private TextView cameraStatus;
    private boolean cameraEnabled;
    private long cameraStartedAt;
    private Button manualButton;
    private Button autoButton;
    private TextView modeStatus;
    private TextView speedValue;
    private final TextView[] sensorReadings = new TextView[SENSOR_LABELS.length];
    private TextView autoOutput;
    private UltrasonicTelemetry telemetry;
    private TextView steeringValue;
    private SteeringWheel wheel;
    private SeekBar speedControl;
    private LinearLayout manualControls;
    private LinearLayout autoControls;
    private Button pedal;
    private Button reversePedal;
    private View activePedal;
    private Button autoStart;
    private Button stop;
    private boolean foreground;
    private int pedalPointer = -1;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_SPLIT_TOUCH,
                WindowManager.LayoutParams.FLAG_SPLIT_TOUCH);
        camera = new CameraStream(this);
        buildScreen();
        if (requiresCamera()) setCameraEnabled(true);
        render();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideBars();
        } else if (foreground && linkState == BluetoothLink.State.CONNECTED) {
            stopDriving();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        foreground = true;
        if (cameraEnabled) startCamera();
        main.post(repeat);
        hideBars();
        finishPairing();
    }

    @Override
    protected void onPause() {
        foreground = false;
        main.removeCallbacks(repeat);
        main.removeCallbacks(telemetryDeadline);
        stopCamera();
        if (picker != null) {
            picker.dismiss();
            picker = null;
        }
        link.disconnect();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        clearPairing();
        camera.close();
        super.onDestroy();
    }

    @SuppressWarnings("deprecation")
    private void hideBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(WindowInsets.Type.systemBars());
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    private void buildScreen() {
        root = column();
        root.setBackgroundColor(BACKGROUND);
        root.setPadding(dp(16), dp(10), dp(16), dp(10));
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            if (Build.VERSION.SDK_INT >= 28 && insets.getDisplayCutout() != null) {
                android.view.DisplayCutout cutout = insets.getDisplayCutout();
                root.setPadding(dp(16) + cutout.getSafeInsetLeft(), dp(10) + cutout.getSafeInsetTop(),
                        dp(16) + cutout.getSafeInsetRight(), dp(10) + cutout.getSafeInsetBottom());
            } else {
                root.setPadding(dp(16), dp(10), dp(16), dp(10));
            }
            return insets;
        });

        LinearLayout header = row();
        header.setGravity(Gravity.CENTER_VERTICAL);
        TextView brand = label(screenTitle(), 22, ACCENT, true);
        brand.setSingleLine(true);
        brand.setLetterSpacing(0.10f);
        header.addView(brand, new LinearLayout.LayoutParams(0, -1, 1));
        connectionStatus = label("연결 안 됨", 13, MUTED, false);
        connectionStatus.setSingleLine(true);
        connectionStatus.setEllipsize(android.text.TextUtils.TruncateAt.END);
        connectionStatus.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        header.addView(connectionStatus, new LinearLayout.LayoutParams(0, -1, 1));
        connectionStatus.setGravity(Gravity.CENTER_VERTICAL | Gravity.RIGHT);
        cameraButton = button("FPV 켜기", 13);
        styleButton(cameraButton, 0xff232c34, TEXT);
        cameraButton.setOnClickListener(view -> toggleCamera());
        if (requiresCamera()) cameraButton.setVisibility(View.GONE);
        LinearLayout.LayoutParams cameraParams = new LinearLayout.LayoutParams(-2, -1);
        cameraParams.leftMargin = dp(12);
        header.addView(cameraButton, cameraParams);
        connectionButton = button("블루투스 연결", 13);
        styleButton(connectionButton, 0xff232c34, ACCENT);
        connectionButton.setOnClickListener(view -> {
            if (pendingPairing != null) cancelPairing("연결 안 됨");
            else if (linkState == BluetoothLink.State.DISCONNECTED) chooseDevice();
            else link.disconnect();
        });
        LinearLayout.LayoutParams connectParams = new LinearLayout.LayoutParams(-2, -1);
        connectParams.leftMargin = dp(12);
        header.addView(connectionButton, connectParams);
        root.addView(header, new LinearLayout.LayoutParams(-1, dp(46)));

        LinearLayout body = row();
        LinearLayout.LayoutParams bodyParams = new LinearLayout.LayoutParams(-1, 0, 1);
        bodyParams.topMargin = dp(10);
        root.addView(body, bodyParams);
        addCard(body, steeringPanel(), 1.2f);
        modeCard = modePanel();
        addCard(body, modeCard, 1.05f);
        addCard(body, pedalPanel(), 1.02f);

        LinearLayout.LayoutParams telemetryParams = new LinearLayout.LayoutParams(-1, dp(53));
        telemetryParams.topMargin = dp(8);
        root.addView(ultrasonicPanel(), telemetryParams);
        setContentView(root);
    }

    private LinearLayout ultrasonicPanel() {
        LinearLayout panel = row();
        for (int i = 0; i < SENSOR_LABELS.length; i++) {
            LinearLayout tile = column();
            tile.setPadding(dp(10), dp(3), dp(10), dp(3));
            tile.setBackground(shape(PANEL, BORDER));
            tile.addView(label(SENSOR_LABELS[i], 11, MUTED, true),
                    new LinearLayout.LayoutParams(-1, dp(17)));
            sensorReadings[i] = label("연결 대기", 13, MUTED, true);
            tile.addView(sensorReadings[i], new LinearLayout.LayoutParams(-1, 0, 1));
            addCard(panel, tile, 1);
        }
        return panel;
    }

    private LinearLayout steeringPanel() {
        LinearLayout card = panel("조향");
        wheel = new SteeringWheel(this, angle -> {
            drive.steer(angle);
            showAngle(angle);
        });
        card.addView(wheel, new LinearLayout.LayoutParams(-1, 0, 1));
        steeringValue = label("0°", 15, MUTED, true);
        steeringValue.setGravity(Gravity.CENTER);
        card.addView(steeringValue, new LinearLayout.LayoutParams(-1, dp(24)));
        return card;
    }

    private LinearLayout modePanel() {
        LinearLayout card = panel("주행 모드");
        modeTitle = card.getChildAt(0);
        modeButtons = row();
        manualButton = button("수동", 14);
        autoButton = button("자율주행", 14);
        manualButton.setOnClickListener(view -> switchMode(false));
        autoButton.setOnClickListener(view -> switchMode(true));
        modeButtons.addView(manualButton, new LinearLayout.LayoutParams(0, -1, 1));
        LinearLayout.LayoutParams autoParams = new LinearLayout.LayoutParams(0, -1, 1.24f);
        autoParams.leftMargin = dp(5);
        modeButtons.addView(autoButton, autoParams);
        View raceAction = createDriveAction();
        if (raceAction != null) {
            manualButton.setVisibility(View.GONE);
            autoButton.setVisibility(View.GONE);
            modeButtons.addView(raceAction, new LinearLayout.LayoutParams(0, -1, 2));
        }
        LinearLayout.LayoutParams modeParams = new LinearLayout.LayoutParams(-1, dp(44));
        modeParams.topMargin = dp(5);
        card.addView(modeButtons, modeParams);

        modeStatus = label("연결 대기", 17, TEXT, true);
        modeStatus.setGravity(Gravity.CENTER);
        modeStatus.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        card.addView(modeStatus, new LinearLayout.LayoutParams(-1, 0, 1));

        cameraPanel = new FrameLayout(this);
        cameraPanel.setBackground(shape(0xff080b10, BORDER));
        cameraPanel.setClipToOutline(true);
        cameraPanel.setVisibility(View.GONE);
        cameraImage = new ImageView(this);
        cameraImage.setScaleType(ImageView.ScaleType.FIT_CENTER);
        cameraImage.setContentDescription("RC카 전방 카메라 영상");
        cameraPanel.addView(cameraImage, new FrameLayout.LayoutParams(-1, -1));
        View overlay = createCameraOverlay();
        if (overlay != null) cameraPanel.addView(overlay, new FrameLayout.LayoutParams(-1, -1));
        cameraStatus = label("", 13, TEXT, true);
        cameraStatus.setPadding(dp(6), 0, dp(6), 0);
        cameraStatus.setGravity(Gravity.CENTER);
        cameraPanel.addView(cameraStatus, new FrameLayout.LayoutParams(-1, -1));
        LinearLayout.LayoutParams previewParams = new LinearLayout.LayoutParams(-1, 0, 1);
        card.addView(cameraPanel, 1, previewParams);

        stop = button("■  정지", 16);
        styleButton(stop, 0xff47282f, RED);
        stop.setOnClickListener(view -> stopDriving());
        card.addView(stop, new LinearLayout.LayoutParams(-1, dp(48)));
        return card;
    }

    private void toggleCamera() {
        stopDriving();
        setCameraEnabled(!cameraEnabled);
    }

    private void setCameraEnabled(boolean enabled) {
        cameraEnabled = enabled;
        cameraButton.setText(cameraEnabled ? "FPV 끄기" : "FPV 켜기");
        cameraPanel.setVisibility(cameraEnabled ? View.VISIBLE : View.GONE);
        modeTitle.setVisibility(cameraEnabled ? View.GONE : View.VISIBLE);
        modeStatus.setLayoutParams(cameraEnabled
                ? new LinearLayout.LayoutParams(-1, dp(24))
                : new LinearLayout.LayoutParams(-1, 0, 1));
        if (cameraEnabled) {
            modeCard.removeView(stop);
            LinearLayout.LayoutParams stopParams = new LinearLayout.LayoutParams(0, -1, 1.14f);
            stopParams.leftMargin = dp(5);
            modeButtons.addView(stop, stopParams);
        } else {
            modeButtons.removeView(stop);
            modeCard.addView(stop, new LinearLayout.LayoutParams(-1, dp(48)));
        }
        LinearLayout.LayoutParams cardParams = (LinearLayout.LayoutParams) modeCard.getLayoutParams();
        cardParams.weight = cameraEnabled ? 2.4f : 1.05f;
        modeCard.setLayoutParams(cardParams);
        if (cameraEnabled && foreground) startCamera();
        else stopCamera();
        render();
    }

    private void startCamera() {
        cameraStartedAt = SystemClock.elapsedRealtime();
        cameraStatus.setText("RC-CAM 영상 연결 중");
        camera.start();
        main.post(cameraRefresh);
    }

    private void stopCamera() {
        main.removeCallbacks(cameraRefresh);
        camera.stop();
        showCameraFrame(null);
    }

    private void updateCamera() {
        CameraStream.Frame frame = camera.latest();
        long now = SystemClock.elapsedRealtime();
        if (frame != null && now - frame.receivedAt >= CameraStream.FRAME_TIMEOUT_MS) frame = null;
        boolean wasLive = displayedFrame != null;
        showCameraFrame(frame);
        String waiting = "영상 대기 · RC-CAM Wi-Fi 확인";
        if (frame == null && now - cameraStartedAt >= CameraStream.FRAME_TIMEOUT_MS) {
            if (!waiting.contentEquals(cameraStatus.getText())) cameraStatus.setText(waiting);
        }
        if (wasLive && frame == null) stopDriving();
        else if (!wasLive && frame != null) render();
    }

    private void showCameraFrame(CameraStream.Frame frame) {
        if (displayedFrame == frame) return;
        displayedFrame = frame;
        cameraImage.setImageBitmap(frame == null ? null : frame.bitmap);
        cameraStatus.setVisibility(frame == null ? View.VISIBLE : View.GONE);
        onCameraFrame(frame);
    }

    private LinearLayout pedalPanel() {
        LinearLayout card = panel("구동");
        FrameLayout actions = new FrameLayout(this);
        card.addView(actions, new LinearLayout.LayoutParams(-1, 0, 1));

        manualControls = column();
        actions.addView(manualControls, new FrameLayout.LayoutParams(-1, -1));
        LinearLayout speedRow = row();
        TextView speedLabel = label("출력 설정", 13, MUTED, false);
        speedRow.addView(speedLabel, new LinearLayout.LayoutParams(0, -1, 1));
        speedValue = label("", 14, ACCENT, true);
        speedRow.addView(speedValue, new LinearLayout.LayoutParams(-2, -1));
        manualControls.addView(speedRow, new LinearLayout.LayoutParams(-1, dp(25)));

        speedControl = new SeekBar(this);
        speedControl.setMax(10);
        speedControl.setProgress(drive.speed() / 10);
        speedControl.setContentDescription("수동 모터 출력");
        speedControl.setProgressTintList(ColorStateList.valueOf(ACCENT));
        speedControl.setThumbTintList(ColorStateList.valueOf(ACCENT));
        speedControl.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int progress, boolean fromUser) {
                drive.setSpeed(progress * 10);
                showSpeed();
            }
            @Override public void onStartTrackingTouch(SeekBar bar) {}
            @Override public void onStopTrackingTouch(SeekBar bar) {}
        });
        manualControls.addView(speedControl, new LinearLayout.LayoutParams(-1, dp(42)));

        LinearLayout pedals = row();
        pedal = button("↑\n가속", 20);
        styleButton(pedal, ACCENT, BACKGROUND);
        pedal.setContentDescription("가속 페달, 누르는 동안 전진");
        pedal.setOnTouchListener((view, event) -> handlePedalTouch(view, event, DriveController.Pedal.FORWARD));
        pedals.addView(pedal, new LinearLayout.LayoutParams(0, -1, 1));

        reversePedal = button("↓\n후진", 20);
        styleButton(reversePedal, 0xff2e3b51, TEXT);
        reversePedal.setContentDescription("후진 페달, 누르는 동안 후진");
        reversePedal.setOnTouchListener((view, event) ->
                handlePedalTouch(view, event, DriveController.Pedal.REVERSE));
        LinearLayout.LayoutParams reverseParams = new LinearLayout.LayoutParams(0, -1, 1);
        reverseParams.leftMargin = dp(6);
        pedals.addView(reversePedal, reverseParams);
        manualControls.addView(pedals, new LinearLayout.LayoutParams(-1, 0, 1));

        autoControls = column();
        actions.addView(autoControls, new FrameLayout.LayoutParams(-1, -1));
        autoOutput = label("모터 출력 대기", 13, MUTED, false);
        autoControls.addView(autoOutput, new LinearLayout.LayoutParams(-1, dp(36)));
        autoStart = button("▶\n주행 재개", 22);
        styleButton(autoStart, ACCENT, BACKGROUND);
        autoStart.setOnClickListener(view -> {
            drive.startAuto();
            render();
        });
        autoControls.addView(autoStart, new LinearLayout.LayoutParams(-1, 0, 1));
        return card;
    }

    private void switchMode(boolean autonomous) {
        DriveController.Mode before = drive.mode();
        drive.selectMode(autonomous);
        if (before != drive.mode()) resetVisualInputs();
        render();
    }

    protected final void stopDriving() {
        drive.stop();
        resetVisualInputs();
        render();
    }

    private boolean handlePedalTouch(View view, MotionEvent event, DriveController.Pedal direction) {
        if (!view.isEnabled()) return false;
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN && activePedal == null) {
            activePedal = view;
            pedalPointer = event.getPointerId(event.getActionIndex());
            view.setPressed(true);
            drive.setPedal(direction);
            return true;
        }
        if (activePedal != view) return true;

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_MOVE:
                int index = event.findPointerIndex(pedalPointer);
                if (index < 0 || event.getX(index) < -dp(20) || event.getY(index) < -dp(20)
                        || event.getX(index) > view.getWidth() + dp(20)
                        || event.getY(index) > view.getHeight() + dp(20)) {
                    releasePedal();
                }
                break;
            case MotionEvent.ACTION_POINTER_UP:
                if (event.getPointerId(event.getActionIndex()) == pedalPointer) releasePedal();
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                releasePedal();
                break;
            default:
                break;
        }
        return true;
    }

    private void releasePedal() {
        clearPedalPress();
        drive.setPedal(DriveController.Pedal.RELEASED);
    }

    private void clearPedalPress() {
        if (activePedal == null) return;
        activePedal.setPressed(false);
        activePedal = null;
        pedalPointer = -1;
    }

    private void resetVisualInputs() {
        wheel.center();
        showAngle(0);
        clearPedalPress();
    }

    private void showAngle(int angle) {
        steeringValue.setText(angle == 0 ? "0°" : (angle < 0 ? "좌 " : "우 ") + Math.abs(angle) + "°");
    }

    private void showSpeed() {
        speedValue.setText(drive.speed() + "%");
    }

    private void onLinkState(BluetoothLink.State state, String detail) {
        linkState = state;
        main.removeCallbacks(telemetryDeadline);
        clearTelemetry(state == BluetoothLink.State.CONNECTED ? "센서 정보 수신 대기" : "연결 대기");
        if (state == BluetoothLink.State.CONNECTED) main.postDelayed(telemetryDeadline, 1800);
        drive.setConnected(state == BluetoothLink.State.CONNECTED);
        resetVisualInputs();
        connectionStatus.setText(detail == null ? "연결 안 됨" : detail);
        connectionStatus.setTextColor(state == BluetoothLink.State.CONNECTED ? ACCENT : MUTED);
        connectionButton.setText(state == BluetoothLink.State.CONNECTED ? "연결 해제"
                : state == BluetoothLink.State.CONNECTING ? "연결 취소"
                : state == BluetoothLink.State.DISCONNECTING ? "연결 종료 중" : "블루투스 연결");
        connectionButton.setEnabled(state != BluetoothLink.State.DISCONNECTING);
        render();
    }

    private void clearTelemetry(String message) {
        telemetry = null;
        for (TextView reading : sensorReadings) {
            reading.setText(message);
            reading.setTextColor(MUTED);
        }
        autoOutput.setText("모터 출력 대기");
        showModeStatus();
    }

    private void onTelemetry(UltrasonicTelemetry frame) {
        telemetry = frame;
        for (int i = 0; i < sensorReadings.length; i++) {
            UltrasonicTelemetry.Reading reading = frame.readings[i];
            TextView view = sensorReadings[i];
            switch (reading.status) {
                case 'D': view.setText(reading.cm + " cm"); break;
                case 'F': view.setText("400 cm 초과"); break;
                case 'N': view.setText("응답 없음 · 연결 확인"); break;
                case 'H': view.setText("ECHO HIGH · 배선 확인"); break;
                case 'P': view.setText("짧은 펄스"); break;
                case 'S': view.setText("측정 지연"); break;
                default: view.setText("측정 대기"); break;
            }
            view.setTextColor(reading.status == 'D' || reading.status == 'F' ? ACCENT
                    : reading.status == 'W' ? MUTED : RED);
        }
        autoOutput.setText("PWM  A " + frame.motorA + "%  B " + frame.motorB + "%");
        showModeStatus();
        main.removeCallbacks(telemetryDeadline);
        main.postDelayed(telemetryDeadline, 1800);
    }

    protected final void render() {
        boolean connected = linkState == BluetoothLink.State.CONNECTED;
        boolean canDrive = connected && (!cameraEnabled || displayedFrame != null) && allowsDriving();
        boolean manual = drive.mode() == DriveController.Mode.MANUAL;
        boolean autoMoving = drive.mode() == DriveController.Mode.AUTO_MOVING;
        manualControls.setVisibility(manual ? View.VISIBLE : View.GONE);
        autoControls.setVisibility(manual ? View.GONE : View.VISIBLE);
        wheel.setEnabled(canDrive && manual);
        wheel.setAlpha(manual ? 1f : 0.38f);
        speedControl.setEnabled(canDrive);
        pedal.setEnabled(canDrive);
        reversePedal.setEnabled(canDrive);
        autoStart.setEnabled(canDrive && !autoMoving);
        stop.setEnabled(connected);
        manualButton.setEnabled(connected);
        autoButton.setEnabled(canDrive);
        styleButton(manualButton, manual ? ACCENT : 0xff252b37, manual ? BACKGROUND : TEXT);
        styleButton(autoButton, manual ? 0xff252b37 : ACCENT, manual ? TEXT : BACKGROUND);
        showModeStatus();
        showSpeed();
    }

    protected String screenTitle() { return "MOONCAR"; }
    protected boolean requiresCamera() { return false; }
    protected boolean allowsDriving() { return true; }
    protected View createDriveAction() { return null; }
    protected View createCameraOverlay() { return null; }
    protected void onControlTick() {}
    protected void onCameraFrame(CameraStream.Frame frame) {}
    protected final boolean hasVehicleLink() { return linkState == BluetoothLink.State.CONNECTED; }
    protected final boolean hasLiveVideo() { return displayedFrame != null; }

    private void showModeStatus() {
        if (linkState != BluetoothLink.State.CONNECTED) modeStatus.setText("연결 대기");
        else if (drive.mode() == DriveController.Mode.MANUAL) modeStatus.setText("수동 조종");
        else if (drive.mode() == DriveController.Mode.AUTO_STOPPED) modeStatus.setText("정지 · 재개 대기");
        else if (telemetry == null) modeStatus.setText("차량 상태 수신 대기");
        else if (telemetry.motorA == 0 && telemetry.motorB == 0) modeStatus.setText("거리 확인 · 정지");
        else if (telemetry.motorA < 0 && telemetry.motorB < 0) modeStatus.setText("후진");
        else if (telemetry.motorA < 0 || telemetry.motorB < 0) modeStatus.setText("제자리 회전");
        else if (telemetry.motorA != telemetry.motorB) modeStatus.setText("코너링");
        else modeStatus.setText("전진");
    }

    private void chooseDevice() {
        List<String> permissions = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= 31 && !hasPermission(Manifest.permission.BLUETOOTH_CONNECT)) {
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT);
        }
        if (!hasPermission(scanPermission())) permissions.add(scanPermission());
        if (permissions.isEmpty()) openPicker();
        else requestPermissions(permissions.toArray(new String[0]), PERMISSION_REQUEST);
    }

    private void openPicker() {
        if (Build.VERSION.SDK_INT >= 31 && !hasPermission(Manifest.permission.BLUETOOTH_CONNECT)) {
            connectionStatus.setText("근처 기기 권한이 필요합니다");
            return;
        }
        BluetoothManager manager = getSystemService(BluetoothManager.class);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        if (adapter == null) {
            connectionStatus.setText("블루투스를 사용할 수 없습니다");
            return;
        }
        if (!adapter.isEnabled()) {
            showPairingSettings("블루투스가 꺼져 있습니다. 설정에서 켜 주세요.");
            return;
        }

        try {
            picker = new BluetoothPicker(this, adapter, this::connectDevice, this::openBluetoothSettings);
            picker.show(hasPermission(scanPermission()));
        } catch (SecurityException error) {
            connectionStatus.setText("근처 기기 권한을 확인해 주세요");
        }
    }

    private boolean hasPermission(String permission) {
        return checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }

    private String scanPermission() {
        return Build.VERSION.SDK_INT >= 31
                ? Manifest.permission.BLUETOOTH_SCAN : Manifest.permission.ACCESS_FINE_LOCATION;
    }

    private void connectDevice(BluetoothDevice device, String name) {
        try {
            if (device.getBondState() == BluetoothDevice.BOND_BONDED) {
                link.connect(device, name);
                return;
            }
            // Finish pairing before opening RFCOMM. The Android PIN screen can pause this activity.
            pendingPairing = device;
            pairingName = name;
            IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
            if (Build.VERSION.SDK_INT >= 33) {
                registerReceiver(pairingReceiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                registerReceiver(pairingReceiver, filter);
            }
            pairingReceiverRegistered = true;
            connectionStatus.setText("페어링 중 · " + name);
            connectionButton.setText("연결 취소");
            main.postDelayed(pairingTimeout, 60_000);
            int bondState = device.getBondState();
            if (bondState == BluetoothDevice.BOND_BONDED) {
                finishPairing();
            } else if (bondState != BluetoothDevice.BOND_BONDING && !device.createBond()) {
                cancelPairing("페어링을 시작할 수 없습니다");
            }
        } catch (SecurityException error) {
            cancelPairing("근처 기기 권한을 확인해 주세요");
        }
    }

    private void finishPairing() {
        if (!foreground || pendingPairing == null) return;
        try {
            if (pendingPairing.getBondState() != BluetoothDevice.BOND_BONDED) return;
            BluetoothDevice device = pendingPairing;
            String name = pairingName;
            clearPairing();
            link.connect(device, name);
        } catch (SecurityException error) {
            cancelPairing("근처 기기 권한을 확인해 주세요");
        }
    }

    private void cancelPairing(String message) {
        clearPairing();
        onLinkState(BluetoothLink.State.DISCONNECTED, message);
    }

    private void clearPairing() {
        main.removeCallbacks(pairingTimeout);
        if (pairingReceiverRegistered) {
            unregisterReceiver(pairingReceiver);
            pairingReceiverRegistered = false;
        }
        pendingPairing = null;
        pairingName = null;
    }

    private void showPairingSettings(String message) {
        new AlertDialog.Builder(this).setTitle("블루투스 설정").setMessage(message)
                .setPositiveButton("설정 열기", (dialog, choice) -> openBluetoothSettings())
                .setNegativeButton("취소", null).show();
    }

    private void openBluetoothSettings() {
        startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS));
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == PERMISSION_REQUEST) openPicker();
    }

    private void addCard(LinearLayout body, LinearLayout card, float width) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, -1, width);
        if (body.getChildCount() > 0) params.leftMargin = dp(10);
        body.addView(card, params);
    }

    private LinearLayout panel(String title) {
        LinearLayout panel = column();
        panel.setPadding(dp(12), dp(8), dp(12), dp(10));
        panel.setBackground(shape(PANEL, BORDER));
        panel.addView(label(title, 15, TEXT, true), new LinearLayout.LayoutParams(-1, dp(26)));
        return panel;
    }

    private LinearLayout row() {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setMotionEventSplittingEnabled(true);
        return row;
    }

    private LinearLayout column() {
        LinearLayout column = new LinearLayout(this);
        column.setOrientation(LinearLayout.VERTICAL);
        column.setMotionEventSplittingEnabled(true);
        return column;
    }

    private TextView label(String text, int size, int color, boolean bold) {
        TextView label = new TextView(this);
        label.setText(text);
        label.setTextSize(size);
        label.setTextColor(color);
        label.setGravity(Gravity.CENTER_VERTICAL);
        if (bold) label.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        return label;
    }

    protected final Button button(String text, int size) {
        Button button = new Button(this);
        button.setAllCaps(false);
        button.setText(text);
        button.setTextSize(size);
        button.setGravity(Gravity.CENTER);
        button.setPadding(dp(8), 0, dp(8), 0);
        button.setMinimumWidth(0);
        button.setMinWidth(0);
        button.setMinimumHeight(0);
        button.setMinHeight(0);
        button.setStateListAnimator(null);
        return button;
    }

    protected final void styleButton(Button button, int background, int text) {
        int[][] states = {{-android.R.attr.state_enabled}, {}};
        button.setTextColor(new ColorStateList(states, new int[]{0xff9ba3b1, text}));
        android.graphics.drawable.StateListDrawable fills = new android.graphics.drawable.StateListDrawable();
        fills.addState(states[0], shape(0xff252a34, BORDER));
        fills.addState(new int[]{android.R.attr.state_pressed}, shape(background == ACCENT ? 0xffabc335 : 0xff424b5a, 0));
        fills.addState(states[1], shape(background, 0));
        button.setBackground(new RippleDrawable(ColorStateList.valueOf(0x30ffffff), fills, shape(0xffffffff, 0)));
    }

    private GradientDrawable shape(int fill, int stroke) {
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(fill);
        shape.setCornerRadius(dp(9));
        if (stroke != 0) shape.setStroke(dp(1), stroke);
        return shape;
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}

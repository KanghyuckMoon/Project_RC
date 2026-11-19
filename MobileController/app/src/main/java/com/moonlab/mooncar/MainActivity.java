package com.moonlab.mooncar;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class MainActivity extends Activity {
    private static final int BACKGROUND = 0xff0d1016;
    private static final int PANEL = 0xff171b24;
    private static final int BORDER = 0xff303644;
    private static final int TEXT = 0xfff1f3f5;
    private static final int MUTED = 0xffa5aebe;
    private static final int ACCENT = 0xffdafa53;
    private static final int RED = 0xffff8d88;
    private static final int PERMISSION_REQUEST = 1;

    private final Handler main = new Handler(Looper.getMainLooper());
    private final BluetoothLink link = new BluetoothLink(this::onLinkState);
    private final DriveController drive = new DriveController(link::send);
    private final Runnable repeat = new Runnable() {
        @Override public void run() {
            drive.repeatManual();
            main.postDelayed(this, 120);
        }
    };

    private BluetoothLink.State linkState = BluetoothLink.State.DISCONNECTED;
    private LinearLayout root;
    private TextView connectionStatus;
    private Button connectionButton;
    private Button manualButton;
    private Button autoButton;
    private TextView modeStatus;
    private TextView speedValue;
    private TextView steeringValue;
    private SteeringWheel wheel;
    private SeekBar speedControl;
    private LinearLayout manualControls;
    private LinearLayout autoControls;
    private Button pedal;
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
        buildScreen();
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
        main.post(repeat);
        hideBars();
    }

    @Override
    protected void onPause() {
        foreground = false;
        main.removeCallbacks(repeat);
        link.disconnect();
        super.onPause();
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
        TextView brand = label("MOONCAR", 22, ACCENT, true);
        brand.setLetterSpacing(0.10f);
        header.addView(brand, new LinearLayout.LayoutParams(0, -1, 1));
        connectionStatus = label("연결 안 됨", 13, MUTED, false);
        connectionStatus.setSingleLine(true);
        connectionStatus.setEllipsize(android.text.TextUtils.TruncateAt.END);
        connectionStatus.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        header.addView(connectionStatus, new LinearLayout.LayoutParams(0, -1, 1));
        connectionStatus.setGravity(Gravity.CENTER_VERTICAL | Gravity.RIGHT);
        connectionButton = button("블루투스 연결", 13);
        styleButton(connectionButton, 0xff232c34, ACCENT);
        connectionButton.setOnClickListener(view -> {
            if (linkState == BluetoothLink.State.DISCONNECTED) chooseDevice();
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
        addCard(body, modePanel(), 1.05f);
        addCard(body, pedalPanel(), 1.02f);

        setContentView(root);
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
        LinearLayout modes = row();
        manualButton = button("수동", 14);
        autoButton = button("자율주행", 14);
        manualButton.setOnClickListener(view -> switchMode(false));
        autoButton.setOnClickListener(view -> switchMode(true));
        modes.addView(manualButton, new LinearLayout.LayoutParams(0, -1, 1));
        LinearLayout.LayoutParams autoParams = new LinearLayout.LayoutParams(0, -1, 1.24f);
        autoParams.leftMargin = dp(5);
        modes.addView(autoButton, autoParams);
        LinearLayout.LayoutParams modeParams = new LinearLayout.LayoutParams(-1, dp(44));
        modeParams.topMargin = dp(5);
        card.addView(modes, modeParams);

        modeStatus = label("연결 대기", 17, TEXT, true);
        modeStatus.setGravity(Gravity.CENTER);
        modeStatus.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        card.addView(modeStatus, new LinearLayout.LayoutParams(-1, 0, 1));

        stop = button("■  정지", 16);
        styleButton(stop, 0xff47282f, RED);
        stop.setOnClickListener(view -> stopDriving());
        card.addView(stop, new LinearLayout.LayoutParams(-1, dp(48)));
        return card;
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

        pedal = button("↑\n가속 페달", 22);
        styleButton(pedal, ACCENT, BACKGROUND);
        pedal.setContentDescription("가속 페달, 누르는 동안 전진");
        pedal.setOnTouchListener((view, event) -> {
            if (!view.isEnabled()) return false;
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    pedalPointer = event.getPointerId(event.getActionIndex());
                    view.setPressed(true);
                    drive.pedal(true);
                    return true;
                case MotionEvent.ACTION_MOVE:
                    int index = event.findPointerIndex(pedalPointer);
                    if (index >= 0 && (event.getX(index) < -dp(20) || event.getY(index) < -dp(20)
                            || event.getX(index) > view.getWidth() + dp(20)
                            || event.getY(index) > view.getHeight() + dp(20))) {
                        releasePedal();
                    }
                    return true;
                case MotionEvent.ACTION_POINTER_UP:
                    if (event.getPointerId(event.getActionIndex()) == pedalPointer) releasePedal();
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    releasePedal();
                    return true;
                default:
                    return true;
            }
        });
        manualControls.addView(pedal, new LinearLayout.LayoutParams(-1, 0, 1));

        autoControls = column();
        actions.addView(autoControls, new FrameLayout.LayoutParams(-1, -1));
        TextView autoLabel = label("차량 FSM", 13, MUTED, false);
        autoControls.addView(autoLabel, new LinearLayout.LayoutParams(-1, dp(36)));
        autoStart = button("▶\n이동 시작", 22);
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

    private void stopDriving() {
        drive.stop();
        resetVisualInputs();
        render();
    }

    private void releasePedal() {
        pedalPointer = -1;
        pedal.setPressed(false);
        drive.pedal(false);
    }

    private void resetVisualInputs() {
        wheel.center();
        showAngle(0);
        pedalPointer = -1;
        pedal.setPressed(false);
    }

    private void showAngle(int angle) {
        steeringValue.setText(angle == 0 ? "0°" : (angle < 0 ? "좌 " : "우 ") + Math.abs(angle) + "°");
    }

    private void showSpeed() {
        speedValue.setText(drive.speed() + "%");
    }

    private void onLinkState(BluetoothLink.State state, String detail) {
        linkState = state;
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

    private void render() {
        boolean connected = linkState == BluetoothLink.State.CONNECTED;
        boolean manual = drive.mode() == DriveController.Mode.MANUAL;
        boolean autoMoving = drive.mode() == DriveController.Mode.AUTO_MOVING;
        manualControls.setVisibility(manual ? View.VISIBLE : View.GONE);
        autoControls.setVisibility(manual ? View.GONE : View.VISIBLE);
        wheel.setEnabled(connected && manual);
        wheel.setAlpha(manual ? 1f : 0.38f);
        speedControl.setEnabled(connected);
        pedal.setEnabled(connected);
        autoStart.setEnabled(connected && !autoMoving);
        stop.setEnabled(connected);
        manualButton.setEnabled(connected);
        autoButton.setEnabled(connected);
        styleButton(manualButton, manual ? ACCENT : 0xff252b37, manual ? BACKGROUND : TEXT);
        styleButton(autoButton, manual ? 0xff252b37 : ACCENT, manual ? TEXT : BACKGROUND);
        modeStatus.setText(!connected ? "연결 대기" : manual ? "수동 조종"
                : autoMoving ? "이동 명령 전송" : "정지 · 이동 대기");
        showSpeed();
    }

    private void chooseDevice() {
        if (Build.VERSION.SDK_INT >= 31
                && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT}, PERMISSION_REQUEST);
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
            List<BluetoothDevice> devices = new ArrayList<>(adapter.getBondedDevices());
            Collections.sort(devices, (first, second) ->
                    deviceLabel(first).compareToIgnoreCase(deviceLabel(second)));
            if (devices.isEmpty()) {
                showPairingSettings("Android 설정에서 먼저 MoonCar 또는 HC-06을 페어링해 주세요.");
                return;
            }
            String[] labels = new String[devices.size()];
            for (int i = 0; i < devices.size(); i++) {
                BluetoothDevice device = devices.get(i);
                labels[i] = deviceLabel(device) + "\n" + device.getAddress();
            }
            new AlertDialog.Builder(this)
                    .setTitle("페어링된 블루투스 기기")
                    .setItems(labels, (dialog, index) -> {
                        BluetoothDevice device = devices.get(index);
                        link.connect(device, deviceLabel(device));
                    })
                    .setNeutralButton("설정", (dialog, choice) -> openBluetoothSettings())
                    .setNegativeButton("취소", null)
                    .show();
        } catch (SecurityException error) {
            connectionStatus.setText("근처 기기 권한을 확인해 주세요");
        }
    }

    private String deviceLabel(BluetoothDevice device) {
        String name = device.getName();
        return name == null || name.trim().isEmpty() ? "이름 없는 기기" : name;
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
        if (requestCode != PERMISSION_REQUEST) return;
        if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
            chooseDevice();
        } else {
            connectionStatus.setText("근처 기기 권한이 필요합니다");
        }
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

    private Button button(String text, int size) {
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

    private void styleButton(Button button, int background, int text) {
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

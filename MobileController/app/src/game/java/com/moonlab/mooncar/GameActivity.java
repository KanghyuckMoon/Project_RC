package com.moonlab.mooncar;

import android.graphics.Bitmap;
import android.os.SystemClock;
import android.view.View;
import android.widget.Button;

public final class GameActivity extends MainActivity {
    private final Race race = new Race();
    private final GateDetector detector = new GateDetector();
    private RaceOverlay overlay;
    private Button raceButton;
    private GateDetector.Marker marker;
    private long lastSampleAt;
    private int[] pixels;

    @Override protected String screenTitle() { return "MOONCAR GAME"; }
    @Override protected boolean requiresCamera() { return true; }
    @Override protected boolean allowsDriving() { return race.state() != Race.State.COUNTDOWN; }

    @Override protected View createDriveAction() {
        raceButton = button("레이스 시작", 15);
        styleButton(raceButton, 0xff72e1ce, 0xff102321);
        raceButton.setEnabled(false);
        raceButton.setOnClickListener(view -> {
            if (race.active()) {
                race.abort();
                stopDriving();
            } else if (raceReady()) {
                race.start(SystemClock.elapsedRealtime());
                marker = null;
                lastSampleAt = 0;
                stopDriving();
            }
            updateRaceDisplay(SystemClock.elapsedRealtime());
        });
        return raceButton;
    }

    @Override protected View createCameraOverlay() {
        overlay = new RaceOverlay(this, race);
        return overlay;
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus) abortInterruptedRace();
    }

    @Override protected void onControlTick() {
        long now = SystemClock.elapsedRealtime();
        if (!raceReady()) abortInterruptedRace();
        Race.State before = race.state();
        race.tick(now);
        if (before != race.state()) render();
        updateRaceDisplay(now);
    }

    @Override protected void onCameraFrame(CameraStream.Frame frame) {
        if (frame == null) {
            overlay.setVideoAspect(0);
            abortInterruptedRace();
            return;
        }
        Bitmap bitmap = frame.bitmap;
        int width = bitmap.getWidth(), height = bitmap.getHeight();
        overlay.setVideoAspect((float) width / height);
        if (race.state() != Race.State.RUNNING || frame.receivedAt - lastSampleAt < 90) return;

        lastSampleAt = frame.receivedAt;
        int count = width * height;
        if (pixels == null || pixels.length < count) pixels = new int[count];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        int gate = race.expectedGate();
        marker = detector.find(pixels, width, height, gate);
        if (marker == null) race.observe(0, 0, 0, frame.receivedAt);
        else race.observe(gate, marker.width, marker.height, frame.receivedAt);
        if (gate != race.expectedGate()) marker = null;
        if (race.state() == Race.State.FINISHED) stopDriving();
        updateRaceDisplay(SystemClock.elapsedRealtime());
    }

    private boolean raceReady() {
        return hasLiveVideo() && hasVehicleLink() && hasWindowFocus();
    }

    private void abortInterruptedRace() {
        if (!race.active()) return;
        race.abort();
        marker = null;
        render();
        updateRaceDisplay(SystemClock.elapsedRealtime());
    }

    private void updateRaceDisplay(long now) {
        String action;
        if (race.active()) action = "레이스 종료";
        else if (race.state() == Race.State.FINISHED || race.state() == Race.State.ABORTED) {
            action = "다시 시작";
        } else action = "레이스 시작";
        if (!action.contentEquals(raceButton.getText())) raceButton.setText(action);
        raceButton.setEnabled(race.active() || raceReady());
        overlay.update(now, marker);
    }
}

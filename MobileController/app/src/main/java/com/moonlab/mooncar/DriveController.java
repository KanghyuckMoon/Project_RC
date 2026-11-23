package com.moonlab.mooncar;

/** App-side state and the byte protocol implemented by car_control.c. */
final class DriveController {
    enum Mode { MANUAL, AUTO_STOPPED, AUTO_MOVING }
    enum Pedal { RELEASED, FORWARD, REVERSE }

    interface Output {
        boolean send(String bytes, boolean recurring);
    }

    private static final int TURN_THRESHOLD = 16;
    private final Output output;
    private boolean connected;
    private Pedal pedal = Pedal.RELEASED;
    private int angle;
    private int speed = 70;
    private Mode mode = Mode.MANUAL;
    private char motion = 'S';

    DriveController(Output output) {
        this.output = output;
    }

    void setConnected(boolean connected) {
        this.connected = connected;
        mode = Mode.MANUAL;
        clearInputs();
    }

    Mode mode() { return mode; }
    int speed() { return speed; }
    char motion() { return motion; }

    void selectMode(boolean autonomous) {
        if (!connected || autonomous == (mode != Mode.MANUAL)) return;
        if (output.send(autonomous ? "SAF" : "SM", false)) {
            clearInputs();
            mode = autonomous ? Mode.AUTO_MOVING : Mode.MANUAL;
        }
    }

    void startAuto() {
        if (connected && mode == Mode.AUTO_STOPPED && output.send("F", false)) {
            mode = Mode.AUTO_MOVING;
        }
    }

    void stop() {
        clearInputs();
        if (connected && output.send("S", false) && mode != Mode.MANUAL) {
            mode = Mode.AUTO_STOPPED;
        }
    }

    void setSpeed(int speed) {
        this.speed = speed;
        updateMotion();
    }

    void steer(int degrees) {
        angle = degrees;
        updateMotion();
    }

    void setPedal(Pedal pedal) {
        this.pedal = pedal;
        updateMotion();
    }

    void repeat(boolean pollSensors) {
        if (!connected) return;
        String query = pollSensors ? "?" : "";
        if (mode == Mode.MANUAL && motion != 'S') output.send(packet(motion) + query, true);
        else if (pollSensors) output.send(query, true);
    }

    private void updateMotion() {
        if (!connected || mode != Mode.MANUAL) return;
        char next = directionCommand();
        if (motion == next) return;
        if (output.send(next == 'S' ? "S" : packet(next), false)) motion = next;
    }

    private char directionCommand() {
        if (speed == 0 || pedal == Pedal.RELEASED) return 'S';

        // The car turns opposite to the firmware's nominal left/right labels.
        boolean left = angle <= -TURN_THRESHOLD;
        boolean right = angle >= TURN_THRESHOLD;
        if (pedal == Pedal.FORWARD) return left ? 'I' : right ? 'G' : 'F';
        return left ? 'J' : right ? 'H' : 'L';
    }

    private String packet(char direction) {
        char throttle = speed == 100 ? 'q' : (char) ('0' + speed / 10);
        return new String(new char[]{throttle, direction});
    }

    private void clearInputs() {
        pedal = Pedal.RELEASED;
        angle = 0;
        motion = 'S';
    }
}

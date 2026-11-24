package com.moonlab.mooncar;

import java.util.Arrays;

final class Race {
    static final int LAPS = 3;
    static final int GATES = 4;
    static final int RACERS = 4;
    static final int FINISH = LAPS * GATES;
    private static final long COUNTDOWN_MS = 3_000;
    private static final long CONFIRM_MS = 150;
    private static final long MAX_SAMPLE_GAP_MS = 450;
    private static final int[][] RIVAL_SPLITS = {
            {5_200, 6_300, 5_800, 6_700},
            {6_400, 5_900, 7_000, 7_700},
            {7_800, 7_300, 6_600, 8_300}
    };
    enum State { READY, COUNTDOWN, RUNNING, FINISHED, ABORTED }

    private State state = State.READY;
    private long goAt;
    private long finishTime;
    private int gates;
    private double approach;
    private int nearFrames;
    private long nearSince;
    private long lastNearFrame;

    State state() { return state; }
    boolean active() { return state == State.COUNTDOWN || state == State.RUNNING; }
    int expectedGate() { return gates % GATES + 1; }
    int lap() { return Math.min(LAPS, gates / GATES + 1); }

    void start(long now) {
        goAt = now + COUNTDOWN_MS;
        state = State.COUNTDOWN;
        gates = 0;
        approach = 0;
        nearFrames = 0;
    }

    void abort() {
        if (active()) state = State.ABORTED;
    }

    void tick(long now) {
        if (state == State.COUNTDOWN && now >= goAt) state = State.RUNNING;
    }

    int countdown(long now) {
        return (int) Math.max(1, (goAt - now + 999) / 1_000);
    }

    long elapsed(long now) {
        return state == State.FINISHED ? finishTime : Math.max(0, now - goAt);
    }

    void observe(int gate, double width, double height, long capturedAt) {
        if (state != State.RUNNING || capturedAt < goAt) return;
        if (gate != expectedGate()) {
            nearFrames = 0;
            return;
        }
        if (height >= .10) approach = Math.max(approach, Math.min(.85, (width - .10) * 5));
        if (width < .27 || height < .15) {
            nearFrames = 0;
            return;
        }
        if (nearFrames == 0 || capturedAt - lastNearFrame > MAX_SAMPLE_GAP_MS) {
            nearFrames = 0;
            nearSince = capturedAt;
        }
        lastNearFrame = capturedAt;
        nearFrames++;
        if (nearFrames < 3 || capturedAt - nearSince < CONFIRM_MS) return;

        gates++;
        nearFrames = 0;
        approach = 0;
        if (gates == FINISH) {
            finishTime = capturedAt - goAt;
            state = State.FINISHED;
        }
    }

    double playerProgress() { return gates + approach; }

    double rivalProgress(int racer, long elapsed) {
        int[] splits = RIVAL_SPLITS[racer - 1];
        int lapTime = rivalLapTime(racer);
        if (elapsed >= (long) lapTime * LAPS) return FINISH;
        int gate = (int) (elapsed / lapTime) * GATES;
        long inLap = elapsed % lapTime;
        for (int split : splits) {
            if (inLap < split) return gate + (double) inLap / split;
            inLap -= split;
            gate++;
        }
        return gate;
    }

    Integer[] standings(long elapsed) {
        Integer[] racers = {0, 1, 2, 3};
        double[] progress = {playerProgress(), rivalProgress(1, elapsed),
                rivalProgress(2, elapsed), rivalProgress(3, elapsed)};
        Arrays.sort(racers, (left, right) -> {
            int order = Double.compare(progress[right], progress[left]);
            if (order != 0 || progress[left] < FINISH) return order;
            return Long.compare(completedAt(left), completedAt(right));
        });
        return racers;
    }

    private long completedAt(int racer) {
        return racer == 0 ? finishTime : (long) rivalLapTime(racer) * LAPS;
    }

    private static int rivalLapTime(int racer) {
        int total = 0;
        for (int split : RIVAL_SPLITS[racer - 1]) total += split;
        return total;
    }
}

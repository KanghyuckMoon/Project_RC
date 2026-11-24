package com.moonlab.mooncar;

import java.util.Arrays;

public final class MoonCarGameTest {
    private static void check(boolean passed, String description) {
        if (!passed) throw new AssertionError(description);
    }

    private static void approach(Race race, int gate, long time) {
        for (int frame = 0; frame < 3; frame++) race.observe(gate, .34, .20, time + 100 * frame);
    }

    public static void main(String[] args) {
        Race race = new Race();
        race.start(1_000);
        approach(race, 1, 1_100);
        race.tick(3_999);
        check(race.state() == Race.State.COUNTDOWN, "Count down for three seconds");
        race.tick(4_000);
        approach(race, 2, 4_100);
        race.observe(1, .32, .20, 4_500);
        race.observe(1, .32, .20, 4_610);
        race.observe(1, .32, .20, 5_300);
        check(race.expectedGate() == 1, "Ignore wrong gates and unconfirmed sightings");
        approach(race, 1, 5_500);
        check(race.expectedGate() == 2, "Accept the expected gate");
        approach(race, 1, 6_000);
        check(race.expectedGate() == 2, "A card cannot be counted twice in a row");
        long time = 7_000;
        for (int i = 1; i < Race.FINISH; i++) {
            approach(race, i % Race.GATES + 1, time);
            time += 7_500;
        }
        check(race.state() == Race.State.FINISHED, "Finish after three ordered laps");
        long finish = race.elapsed(time);
        check(finish == race.elapsed(time + 100_000), "Freeze time at the finish");
        check(Arrays.equals(race.standings(finish), new Integer[]{1, 0, 2, 3}),
                "Rank ties at the finish by completion time");
        race.start(time + 1);
        race.abort();
        check(race.state() == Race.State.ABORTED && race.expectedGate() == 1, "Restart and interrupt");

        GateDetector detector = new GateDetector();
        int pink = 0xffe916b9, cyan = 0xff00c9e8, yellow = 0xffefd414;
        int[][] gates = {{pink, cyan}, {cyan, pink}, {yellow, cyan}, {cyan, yellow}};
        final int width = 320, height = 240;
        int[] frame = new int[width * height];
        for (int i = 0; i < gates.length; i++) {
            Arrays.fill(frame, 0xff6b6b6b);
            for (int y = 75; y < 130; y++) {
                Arrays.fill(frame, y * width + 90, y * width + 150, gates[i][0]);
                Arrays.fill(frame, y * width + 150, y * width + 210, gates[i][1]);
            }
            for (int j = 0; j < gates.length; j++) {
                GateDetector.Marker card = detector.find(frame, width, height, j + 1);
                check((card != null) == (i == j), "Distinguish checkpoint color blocks");
                if (card != null) check(card.width >= .27 && card.height >= .15, "Detect an approaching card");
            }
        }
        System.out.println("MoonCarGame checkpoint, countdown, finish and ranking checks passed");
    }
}

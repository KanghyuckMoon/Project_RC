package com.moonlab.mooncar;

final class UltrasonicTelemetry {
    static final class Reading {
        final char status;
        final int cm;

        Reading(char status, int cm) {
            this.status = status;
            this.cm = cm;
        }
    }

    final Reading[] readings;
    final int motorA;
    final int motorB;

    private UltrasonicTelemetry(Reading[] readings, int motorA, int motorB) {
        this.readings = readings;
        this.motorA = motorA;
        this.motorB = motorB;
    }

    private static UltrasonicTelemetry parse(String line) {
        String[] fields = line.split(",", -1);
        if (fields.length != 9 || !fields[0].equals("@US")) return null;
        Reading[] readings = new Reading[3];
        try {
            for (int i = 0; i < readings.length; i++) {
                String code = fields[1 + 2 * i];
                if (code.length() != 1 || "WDFNHPS".indexOf(code.charAt(0)) < 0) return null;
                char status = code.charAt(0);
                int cm = Integer.parseInt(fields[2 + 2 * i]);
                if (status == 'D' ? cm < 2 || cm > 400 : cm != (status == 'F' ? 400 : 0)) {
                    return null;
                }
                readings[i] = new Reading(status, cm);
            }
            int a = Integer.parseInt(fields[7]);
            int b = Integer.parseInt(fields[8]);
            if (a < -100 || a > 100 || b < -100 || b > 100) return null;
            return new UltrasonicTelemetry(readings, a, b);
        } catch (NumberFormatException error) {
            return null;
        }
    }

    /** Receives arbitrary RFCOMM chunks and resynchronizes at every frame marker. */
    static final class Decoder {
        private final StringBuilder line = new StringBuilder(48);

        UltrasonicTelemetry receive(byte[] bytes, int count) {
            UltrasonicTelemetry latest = null;
            for (int i = 0; i < count; i++) {
                char value = (char) (bytes[i] & 0xff);
                if (value == '@') {
                    line.setLength(0);
                    line.append(value);
                } else if (line.length() == 0) {
                    continue;
                } else if (value == '\n') {
                    UltrasonicTelemetry frame = parse(line.toString());
                    if (frame != null) latest = frame;
                    line.setLength(0);
                } else if (value < ' ' || value > '~' || line.length() >= 48) {
                    line.setLength(0);
                } else {
                    line.append(value);
                }
            }
            return latest;
        }
    }
}

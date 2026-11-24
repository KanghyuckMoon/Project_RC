package com.moonlab.mooncar;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;

/** Reads the length-delimited JPEG parts emitted by the ESP32-CAM server. */
final class MjpegReader {
    private static final String BOUNDARY = "--rc-cam-frame";
    private static final int MAX_HEADER_LINE = 256;
    private static final int MAX_FRAME_BYTES = 512 * 1024;
    private final InputStream input;
    private byte[] frame = new byte[32 * 1024];

    MjpegReader(InputStream input) {
        this.input = input;
    }

    int readFrame() throws IOException {
        String line = readLine();
        if (line.isEmpty()) line = readLine();
        if (!BOUNDARY.equals(line)) throw new IOException("Invalid MJPEG boundary");

        int length = -1;
        for (int i = 0; i < 16; i++) {
            line = readLine();
            if (line.isEmpty()) {
                if (length <= 0 || length > MAX_FRAME_BYTES) throw new IOException("Invalid JPEG length");
                if (length > frame.length) frame = new byte[length];
                for (int offset = 0; offset < length;) {
                    int count = input.read(frame, offset, length - offset);
                    if (count < 0) throw new EOFException("Incomplete JPEG");
                    offset += count;
                }
                return length;
            }
            int separator = line.indexOf(':');
            if (separator >= 0 && "Content-Length".equalsIgnoreCase(line.substring(0, separator))) {
                try {
                    length = Integer.parseInt(line.substring(separator + 1).trim());
                } catch (NumberFormatException error) {
                    throw new IOException("Invalid JPEG length", error);
                }
            }
        }
        throw new IOException("Too many MJPEG headers");
    }

    byte[] frameBytes() {
        return frame;
    }

    private String readLine() throws IOException {
        StringBuilder line = new StringBuilder();
        for (int i = 0; i < MAX_HEADER_LINE; i++) {
            int value = input.read();
            if (value < 0) throw new EOFException("MJPEG stream ended");
            if (value == '\n') return line.toString();
            if (value != '\r') line.append((char) value);
        }
        throw new IOException("MJPEG header line is too long");
    }
}

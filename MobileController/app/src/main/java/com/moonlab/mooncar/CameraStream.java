package com.moonlab.mooncar;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.SystemClock;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Proxy;
import java.net.URL;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

final class CameraStream {
    static final long FRAME_TIMEOUT_MS = 1_500;
    private static final String STREAM_URL = "http://192.168.4.1:81/stream";
    private static final int CONNECT_TIMEOUT_MS = 1_500;
    private static final int RETRY_DELAY_MS = 800;

    private final ConnectivityManager connectivity;
    private final ExecutorService reader = Executors.newSingleThreadExecutor();
    private Session session;

    CameraStream(Context context) {
        connectivity = context.getSystemService(ConnectivityManager.class);
    }

    // Session selection and frame display belong to the main thread.
    void start() {
        stop();
        Session next = new Session();
        session = next;
        reader.execute(next::run);
    }

    void stop() {
        if (session == null) return;
        session.cancelled = true;
        session = null;
    }

    void close() {
        stop();
        reader.shutdownNow();
    }

    Frame latest() {
        return session == null ? null : session.latest;
    }

    @SuppressWarnings("deprecation")
    private Network wifiNetwork() throws IOException {
        // A camera access point without internet may coexist with the cellular default.
        for (Network network : connectivity.getAllNetworks()) {
            NetworkCapabilities capabilities = connectivity.getNetworkCapabilities(network);
            if (capabilities != null && capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                return network;
            }
        }
        throw new IOException("Wi-Fi is disconnected");
    }

    static final class Frame {
        final Bitmap bitmap;
        final long receivedAt;

        Frame(Bitmap bitmap, long receivedAt) {
            this.bitmap = bitmap;
            this.receivedAt = receivedAt;
        }
    }

    private final class Session {
        volatile boolean cancelled;
        volatile Frame latest;

        void run() {
            while (!cancelled) {
                try {
                    receive();
                } catch (IOException | SecurityException error) {
                    // Keep trying when the camera starts or its Wi-Fi is rejoined.
                } finally {
                    latest = null;
                }
                if (cancelled) return;
                try {
                    Thread.sleep(RETRY_DELAY_MS);
                } catch (InterruptedException error) {
                    Thread.currentThread().interrupt();
                    return;
                }
            }
        }

        private void receive() throws IOException {
            HttpURLConnection connection = (HttpURLConnection) wifiNetwork()
                    .openConnection(new URL(STREAM_URL), Proxy.NO_PROXY);
            try {
                connection.setConnectTimeout(CONNECT_TIMEOUT_MS);
                connection.setReadTimeout((int) FRAME_TIMEOUT_MS);
                connection.setInstanceFollowRedirects(false);
                connection.setUseCaches(false);
                if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
                    throw new IOException("Camera stream unavailable");
                }
                try (InputStream input = new BufferedInputStream(connection.getInputStream())) {
                    MjpegReader mjpeg = new MjpegReader(input);
                    while (!cancelled) {
                        int length = mjpeg.readFrame();
                        long receivedAt = SystemClock.elapsedRealtime();
                        Bitmap bitmap = BitmapFactory.decodeByteArray(mjpeg.frameBytes(), 0, length);
                        if (bitmap != null && !cancelled) latest = new Frame(bitmap, receivedAt);
                    }
                }
            } finally {
                connection.disconnect();
            }
        }
    }
}

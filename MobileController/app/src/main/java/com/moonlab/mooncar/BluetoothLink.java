package com.moonlab.mooncar;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.UUID;

final class BluetoothLink {
    enum State { DISCONNECTED, CONNECTING, CONNECTED, DISCONNECTING }

    interface Listener {
        void onLinkState(State state, String detail);
    }

    private static final UUID SPP = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Listener listener;
    private Session current;

    BluetoothLink(Listener listener) {
        this.listener = listener;
    }

    // The activity calls these methods on the main thread.
    void connect(BluetoothDevice device, String label) {
        if (current != null) return;
        try {
            Session session = new Session(device.createRfcommSocketToServiceRecord(SPP), label);
            current = session;
            listener.onLinkState(State.CONNECTING, label);
            new Thread(session::run, "mooncar-rfcomm").start();
        } catch (IOException | SecurityException error) {
            listener.onLinkState(State.DISCONNECTED, "블루투스 연결을 열 수 없습니다");
        }
    }

    boolean send(String bytes, boolean recurring) {
        return current != null && current.enqueue(bytes, recurring);
    }

    void disconnect() {
        Session session = current;
        if (session == null) return;
        listener.onLinkState(State.DISCONNECTING, session.label);
        if (!session.requestStop()) session.finish(null);
    }

    private final class Session {
        private final BluetoothSocket socket;
        private final String label;
        private final ArrayDeque<Command> queue = new ArrayDeque<>();
        private boolean ready;
        private boolean stopping;
        private boolean closed;

        Session(BluetoothSocket socket, String label) {
            this.socket = socket;
            this.label = label;
        }

        synchronized boolean enqueue(String bytes, boolean recurring) {
            if (!ready || stopping || closed) return false;

            Command tail = queue.peekLast();
            if (tail != null && tail.recurring) queue.removeLast();
            if (queue.size() >= 8) {
                if (!recurring) finish("명령 전송 지연으로 연결이 종료됐습니다");
                return false;
            }
            queue.addLast(new Command(bytes, recurring));
            notifyAll();
            return true;
        }

        synchronized boolean requestStop() {
            if (!ready || closed) return false;
            stopping = true;
            queue.clear();
            queue.addLast(new Command("S", false));
            notifyAll();
            return true;
        }

        private synchronized Command nextCommand() throws InterruptedException {
            while (queue.isEmpty() && !closed) wait();
            return queue.pollFirst();
        }

        void run() {
            try {
                Runnable connectTimeout = () -> finish("블루투스 연결 시간이 초과됐습니다");
                main.postDelayed(connectTimeout, 10_000);
                try {
                    socket.connect();
                } finally {
                    main.removeCallbacks(connectTimeout);
                }

                OutputStream output = socket.getOutputStream();
                // A reconnect always starts from a stopped manual state.
                write(output, "SM");
                synchronized (this) {
                    if (closed) return;
                    ready = true;
                }

                main.post(() -> {
                    synchronized (this) {
                        if (current != this || closed || stopping) return;
                    }
                    listener.onLinkState(State.CONNECTED, label);
                });

                InputStream input = socket.getInputStream();
                new Thread(() -> watchConnection(input), "mooncar-rfcomm-read").start();

                Command command;
                while ((command = nextCommand()) != null) {
                    write(output, command.bytes);
                    synchronized (this) {
                        if (stopping && queue.isEmpty()) break;
                    }
                }
                finish(null);
            } catch (IOException | InterruptedException | SecurityException error) {
                finish("블루투스 연결이 끊겼습니다");
            }
        }

        private void write(OutputStream output, String bytes) throws IOException {
            Runnable timeout = () -> finish("명령 전송 시간이 초과됐습니다");
            main.postDelayed(timeout, 400);
            try {
                output.write(bytes.getBytes(StandardCharsets.US_ASCII));
            } finally {
                main.removeCallbacks(timeout);
            }
        }

        private void watchConnection(InputStream input) {
            try {
                byte[] buffer = new byte[64];
                while (input.read(buffer) != -1) {
                    // The firmware currently sends no vehicle telemetry.
                }
            } catch (IOException ignored) {
                // A closed or failed connection is handled below.
            }
            finish("블루투스 연결이 끊겼습니다");
        }

        void finish(String reason) {
            synchronized (this) {
                if (closed) return;
                closed = true;
                queue.clear();
                notifyAll();
            }
            try {
                socket.close();
            } catch (IOException ignored) {
                // The session is already closed to new commands.
            }
            main.post(() -> {
                if (current == this) {
                    current = null;
                    listener.onLinkState(State.DISCONNECTED, reason);
                }
            });
        }
    }

    private static final class Command {
        final String bytes;
        final boolean recurring;

        Command(String bytes, boolean recurring) {
            this.bytes = bytes;
            this.recurring = recurring;
        }
    }
}

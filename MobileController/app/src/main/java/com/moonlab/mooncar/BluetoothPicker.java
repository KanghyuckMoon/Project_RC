package com.moonlab.mooncar;

import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.widget.ArrayAdapter;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

final class BluetoothPicker {
    interface Selection {
        void connect(BluetoothDevice device, String name);
    }

    private final Activity activity;
    private final BluetoothAdapter adapter;
    private final Selection selection;
    private final Runnable openSettings;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final List<DeviceRow> devices = new ArrayList<>();
    private final Map<String, DeviceRow> devicesByAddress = new HashMap<>();
    private final Runnable discoveryDeadline = this::stopSearch;
    private final Runnable leDeadline = () -> {
        stopLe();
        updateTitle();
    };
    private final BroadcastReceiver discoveryReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (dialog == null) return;
            if (BluetoothDevice.ACTION_FOUND.equals(intent.getAction())
                    || BluetoothDevice.ACTION_NAME_CHANGED.equals(intent.getAction())) {
                BluetoothDevice device = deviceFrom(intent);
                if (device != null) addDevice(device, intent.getStringExtra(BluetoothDevice.EXTRA_NAME));
            } else if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(intent.getAction())) {
                classicSearching = false;
                updateTitle();
            }
        }
    };
    private final ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            main.post(() -> {
                if (scanner == null) return;
                String name = result.getScanRecord() == null ? null : result.getScanRecord().getDeviceName();
                addDevice(result.getDevice(), name);
            });
        }

        @Override
        public void onScanFailed(int errorCode) {
            main.post(() -> {
                if (scanner == null) return;
                searchFailed = true;
                stopLe();
                updateTitle();
            });
        }
    };

    private ArrayAdapter<DeviceRow> rows;
    private AlertDialog dialog;
    private BluetoothLeScanner scanner;
    private boolean receiverRegistered;
    private boolean classicSearching;
    private boolean searchFailed;

    BluetoothPicker(Activity activity, BluetoothAdapter adapter, Selection selection, Runnable openSettings) {
        this.activity = activity;
        this.adapter = adapter;
        this.selection = selection;
        this.openSettings = openSettings;
    }

    void show(boolean canScan) {
        rows = new ArrayAdapter<>(activity, android.R.layout.simple_list_item_1, devices);
        for (BluetoothDevice device : adapter.getBondedDevices()) addDevice(device, null);
        dialog = new AlertDialog.Builder(activity)
                .setTitle("주변 블루투스 기기")
                .setAdapter(rows, null)
                .setNegativeButton("닫기", null)
                .setNeutralButton("설정", (view, choice) -> {
                    dismiss();
                    openSettings.run();
                })
                .create();
        dialog.setOnDismissListener(view -> {
            stopSearch();
            dialog = null;
        });
        dialog.show();
        dialog.getListView().setOnItemClickListener((list, view, position, id) -> {
            DeviceRow row = devices.get(position);
            if (row.bleOnly) {
                Toast.makeText(activity, "이 BLE 기기는 RC카 연결을 지원하지 않습니다",
                        Toast.LENGTH_SHORT).show();
                return;
            }
            dismiss();
            selection.connect(row.device, row.name);
        });
        if (canScan) {
            startSearch();
        } else {
            dialog.setTitle("검색 권한 필요 · 페어링된 기기 " + devices.size() + "대");
        }
    }

    void dismiss() {
        if (dialog != null) {
            stopSearch();
            dialog.dismiss();
            dialog = null;
        }
    }

    private void startSearch() {
        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_FOUND);
        filter.addAction(BluetoothDevice.ACTION_NAME_CHANGED);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);
        try {
            if (Build.VERSION.SDK_INT >= 33) {
                // Bluetooth broadcasts originate from the system Bluetooth app.
                activity.registerReceiver(discoveryReceiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                activity.registerReceiver(discoveryReceiver, filter);
            }
            receiverRegistered = true;
            classicSearching = adapter.startDiscovery() || adapter.isDiscovering();
            searchFailed = !classicSearching;
        } catch (SecurityException error) {
            searchFailed = true;
        }
        if (activity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            try {
                scanner = adapter.getBluetoothLeScanner();
                if (scanner != null) {
                    scanner.startScan(null, new ScanSettings.Builder()
                            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCallback);
                    main.postDelayed(leDeadline, 12_000);
                } else {
                    searchFailed = true;
                }
            } catch (IllegalStateException | SecurityException error) {
                scanner = null;
                searchFailed = true;
            }
        }
        main.postDelayed(discoveryDeadline, 25_000);
        updateTitle();
    }

    private void stopSearch() {
        main.removeCallbacks(discoveryDeadline);
        if (receiverRegistered) {
            receiverRegistered = false;
            activity.unregisterReceiver(discoveryReceiver);
            try {
                if (adapter.isDiscovering()) adapter.cancelDiscovery();
            } catch (SecurityException ignored) {
                // Permission may have been revoked during discovery.
            }
        }
        classicSearching = false;
        stopLe();
        updateTitle();
    }

    private void stopLe() {
        main.removeCallbacks(leDeadline);
        if (scanner == null) return;
        BluetoothLeScanner active = scanner;
        scanner = null;
        try {
            active.stopScan(scanCallback);
        } catch (IllegalStateException | SecurityException ignored) {
            // Bluetooth may have been turned off during the scan.
        }
    }

    private void addDevice(BluetoothDevice device, String advertisedName) {
        try {
            String address = device.getAddress();
            String name = device.getName();
            if (name == null || name.trim().isEmpty()) name = advertisedName;
            boolean bonded = device.getBondState() == BluetoothDevice.BOND_BONDED;
            boolean bleOnly = device.getType() == BluetoothDevice.DEVICE_TYPE_LE;
            DeviceRow row = devicesByAddress.get(address);
            if (row != null) {
                if (name == null || name.trim().isEmpty()) name = row.name;
                if (!row.name.equals(name) || row.bonded != bonded || row.bleOnly != bleOnly) {
                    row.name = name;
                    row.bonded = bonded;
                    row.bleOnly = bleOnly;
                    rows.notifyDataSetChanged();
                }
                return;
            }
            if (name == null || name.trim().isEmpty()) name = "이름 없는 기기";
            row = new DeviceRow(device, name, bonded, bleOnly);
            devicesByAddress.put(address, row);
            devices.add(row);
            rows.notifyDataSetChanged();
            updateTitle();
        } catch (SecurityException ignored) {
            // Ignore results that arrive after permission is revoked.
        }
    }

    private void updateTitle() {
        if (dialog == null) return;
        String status = classicSearching || scanner != null ? "주변 기기 검색 중"
                : searchFailed ? "일부 또는 전체 검색 실패" : "검색 완료";
        dialog.setTitle(status + " · " + devices.size() + "대");
    }

    @SuppressWarnings("deprecation")
    static BluetoothDevice deviceFrom(Intent intent) {
        if (Build.VERSION.SDK_INT >= 33) {
            return intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice.class);
        }
        return intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
    }

    private static final class DeviceRow {
        final BluetoothDevice device;
        String name;
        boolean bonded;
        boolean bleOnly;

        DeviceRow(BluetoothDevice device, String name, boolean bonded, boolean bleOnly) {
            this.device = device;
            this.name = name;
            this.bonded = bonded;
            this.bleOnly = bleOnly;
        }

        @Override public String toString() {
            return name + "\n" + device.getAddress() + "  ·  "
                    + (bonded ? "페어링됨" : "페어링 안 됨")
                    + (bleOnly ? "  ·  BLE 전용" : "");
        }
    }
}

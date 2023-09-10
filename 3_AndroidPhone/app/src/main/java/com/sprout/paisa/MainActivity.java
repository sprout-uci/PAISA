package com.sprout.paisa;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Parcel;
import android.util.Base64;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.InvalidKeyException;
import java.security.KeyFactory;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PublicKey;
import java.security.Signature;
import java.security.SignatureException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.X509EncodedKeySpec;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.Dictionary;
import java.util.Hashtable;
import java.util.List;
import java.util.Locale;


public class MainActivity extends AppCompatActivity {

    protected static final String TAG = "MonitoringActivity";

    private static final String LOG_TAG = "AndroidExample";

    private static final int MY_REQUEST_CODE = 123;

    private WifiManager wifiManager;

    private Button buttonScan;

    private TextView textViewScanResults;

    private WifiBroadcastReceiver wifiReceiver;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        this.wifiManager = (WifiManager) this.getApplicationContext().getSystemService(Context.WIFI_SERVICE);

        // Instantiate broadcast receiver
        this.wifiReceiver = new WifiBroadcastReceiver();

        // Register the receiver
        registerReceiver(wifiReceiver, new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION));

        //
        this.buttonScan = (Button) this.findViewById(R.id.button_scan);

        this.textViewScanResults = (TextView) this.findViewById(R.id.textView_scanResults);
//        this.linearLayoutScanResults = (LinearLayout) this.findViewById(R.id.linearLayout_scanResults);

//        this.buttonState.setOnClickListener(new View.OnClickListener() {
//
//            @Override
//            public void onClick(View v) {
//                showWifiState();
//            }
//        });

        this.buttonScan.setOnClickListener(v -> askAndStartScanWifi());
    }


    private void askAndStartScanWifi() {

        // With Android Level >= 23, you have to ask the user
        // for permission to Call.
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) { // 23
            int permission1 = ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION);

            // Check for permissions
            if (permission1 != PackageManager.PERMISSION_GRANTED) {

                Log.d(LOG_TAG, "Requesting Permissions");

                // Request permissions
                ActivityCompat.requestPermissions(this,
                        new String[]{
                                Manifest.permission.ACCESS_COARSE_LOCATION,
                                Manifest.permission.ACCESS_FINE_LOCATION,
                                Manifest.permission.ACCESS_WIFI_STATE,
                                Manifest.permission.ACCESS_NETWORK_STATE
                        }, MY_REQUEST_CODE);
                return;
            }
//            Log.d(LOG_TAG, "Permissions Already Granted");
        }
        this.doStartScanWifi();
    }

    private void doStartScanWifi() {
        this.wifiManager.startScan();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
//        Log.d(LOG_TAG, "onRequestPermissionsResult");

        switch (requestCode) {
            case MY_REQUEST_CODE: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // permission was granted
                    Log.d(LOG_TAG, "Permission Granted: " + permissions[0]);

                    // Start Scan Wifi.
                    this.doStartScanWifi();
                } else {
                    // Permission denied, boo! Disable the
                    // functionality that depends on this permission.
                    Log.d(LOG_TAG, "Permission Denied: " + permissions[0]);
                }
                break;
            }
            // Other 'case' lines to check for other
            // permissions this app might request.
        }
    }

    @Override
    protected void onStop() {
        this.unregisterReceiver(this.wifiReceiver);
        super.onStop();
    }


    // Define class to listen to broadcasts
    class WifiBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
//            Log.d(LOG_TAG, "onReceive()");

            Toast.makeText(MainActivity.this, "Scan Complete!", Toast.LENGTH_SHORT).show();

            boolean ok = intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false);

            if (ok) {
//                Log.d(LOG_TAG, "Scan OK");
                long startTime = System.currentTimeMillis();

                if (ActivityCompat.checkSelfPermission(MainActivity.this, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                    // TODO: Consider calling
                    //    ActivityCompat#requestPermissions
                    // here to request the missing permissions, and then overriding
                    //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                    //                                          int[] grantResults)
                    // to handle the case where the user grants the permission. See the documentation
                    // for ActivityCompat#requestPermissions for more details.
                    return;
                }
                List<ScanResult> list = wifiManager.getScanResults();

//                MainActivity.this.showNetworks(list);
                MainActivity.this.showNetworksDetails(list);
                long endTime = System.currentTimeMillis();

                Log.d(TAG, "[1]" + (endTime - startTime));
            } else {
                Log.d(LOG_TAG, "Scan not OK");
            }

        }
    }
//
//    private void showNetworks(List<ScanResult> results) {
//        this.linearLayoutScanResults.removeAllViews();
//
////        Log.i(TAG, "The first beacon I see is about 500" + " meters away.");
//
//        for (final ScanResult result : results) {
//            final String networkCapabilities = result.capabilities;
//            final String networkSSID = result.SSID; // Network Name.
//
//
//
////            List<ScanResult.InformationElement> informationElements = null;
////            try {
////                Method method = result.getClass().getMethod("getInformationElements");
////                informationElements = (List<ScanResult.InformationElement>) method.invoke(result);
////            } catch (Exception e) {
////                e.printStackTrace();
////            }
////            if (informationElements != null) {
////                for (ScanResult.InformationElement informationElement : informationElements) {
////                    byte[] bytes = informationElement.getBytes();
////                    if (bytes != null) {
////                        int offset = 0;
////                        while (offset < bytes.length - 1) {
////                            int elementId = bytes[offset] & 0xFF;
////                            int length = bytes[offset + 1] & 0xFF;
////                            if (elementId == 221) { // Vendor-specific element
////                                int vendorId = ((bytes[offset + 2] & 0xFF) << 16) |
////                                        ((bytes[offset + 3] & 0xFF) << 8) |
////                                        (bytes[offset + 4] & 0xFF);
////                                if (vendorId == 0x004c) { // Apple
////                                    // Parse the vendor-specific data here
////                                    byte[] vendorSpecificData = Arrays.copyOfRange(bytes, offset + 5, offset + length + 1);
////                                }
////                            }
////                            offset += length + 2; // Move to next element
////                        }
////                    }
////                }
////            }
//
//
//
//
//            Button button = new Button(this);
//
//            button.setText(networkSSID + " (" + networkCapabilities + ")");
//            this.linearLayoutScanResults.addView(button);
//
//            button.setOnClickListener(new View.OnClickListener() {
//                @Override
//                public void onClick(View v) {
//                    String networkCapabilities = result.capabilities;
//                    connectToNetwork(networkCapabilities, networkSSID);
//                }
//            });
//        }
//    }

    class DeviceProfile extends AsyncTask<String, Void, String>
    {
        private byte[] n_dev;
        private byte[] time_cur;
        private byte[] sig;
        private byte[] attest_result;
        private byte[] time_attest;
        private String shortenedUrl;
        private long startTime;

        @Override
        protected String doInBackground(String... params) {
            try {
                startTime = System.currentTimeMillis();

                shortenedUrl = params[0];
                URL url = new URL(shortenedUrl);
                n_dev = Base64.decode(params[1], Base64.DEFAULT);
                time_cur = Base64.decode(params[2], Base64.DEFAULT);
                sig = Base64.decode(params[3], Base64.DEFAULT);
                attest_result = Base64.decode(params[4], Base64.DEFAULT);
                time_attest = Base64.decode(params[5], Base64.DEFAULT);

                HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();
                urlConnection.setRequestMethod("GET");
                urlConnection.connect();

                InputStream inputStream = urlConnection.getInputStream();
                BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));

                StringBuilder stringBuilder = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) {
                    stringBuilder.append(line + "\n");
                }

                inputStream.close();
                urlConnection.disconnect();

                return stringBuilder.toString();
            } catch (IOException e) {
                e.printStackTrace();
                return null;
            }
        }

        protected String print(byte[] bytes) {
            StringBuilder sb = new StringBuilder();
            sb.append("[ ");
            for (byte b : bytes) {
                sb.append(String.format("0x%02X ", b));
            }
            sb.append("]");
            return sb.toString();
        }

        @Override
        protected void onPostExecute(String deviceProfile) {
            // Verification of Announced message

            StringBuilder sb = new StringBuilder();

            String[] profile = deviceProfile.split("\n");
            Dictionary<String, String> dict = new Hashtable<>();

            for (String line: profile) {
                dict.put(line.split(":", 2)[0], line.split(":", 2)[1]);
            }
            // location
//            Log.d(TAG, "Location: " + dict.get("location"));

            // verify signature of device profile
            byte[] profileSigBody = deviceProfile.split("signature_of_manifest")[0].replaceAll("\\\\n", "\n").replaceAll("\n\n", "\n").getBytes(StandardCharsets.UTF_8);
            String profileSig = dict.get("signature_of_manifest").replaceAll("\\\\n", "\n");
            String mfrCertString = dict.get("certificate_of_manufacturer");
            mfrCertString = mfrCertString.replaceAll("\\\\n", "\n");
            byte[] profileSigByte = Base64.decode(profileSig, Base64.DEFAULT);

            try {
                X509Certificate mfrCert = (X509Certificate) CertificateFactory.getInstance("X.509")
                        .generateCertificate(new ByteArrayInputStream(mfrCertString.getBytes(StandardCharsets.UTF_8)));
                PublicKey mfrPublicKey = mfrCert.getPublicKey();

                KeyFactory keyFactory = KeyFactory.getInstance("EC");
                X509EncodedKeySpec publicKeySpec = new X509EncodedKeySpec(mfrPublicKey.getEncoded());
                PublicKey signaturePublicKey = keyFactory.generatePublic(publicKeySpec);
                Signature signatureVerifier = Signature.getInstance("SHA256withECDSA");
                signatureVerifier.initVerify(signaturePublicKey);
                signatureVerifier.update(profileSigBody);
                boolean isSignatureValid = signatureVerifier.verify(profileSigByte);
                sb.append("Manifest Verification: " + (isSignatureValid==true?"PASS":"FAIL") + "\n");
            } catch (CertificateException | InvalidKeySpecException | NoSuchAlgorithmException |
                     SignatureException | InvalidKeyException e) {
                throw new RuntimeException(e);
            }

            // verify signature of announced message from IoT device
            String devCertString = dict.get("certificate_of_device");
            devCertString = devCertString.replaceAll("\\\\n", "\n");
            int devID = Integer.parseInt(dict.get("device_id"));

            Date dateAttTs = new Date((long)ByteBuffer.wrap(time_attest).order(ByteOrder.LITTLE_ENDIAN).getInt()*1000); // convert epoch time to Date object
            SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault());
            String formattedAttTs = dateFormat.format(dateAttTs); // format date as string
            Date dateTs = new Date((long)ByteBuffer.wrap(time_cur).order(ByteOrder.LITTLE_ENDIAN).getInt()*1000); // convert epoch time to Date object
            String formattedTs = dateFormat.format(dateTs); // format date as string


            // signature: [n_dev(32) || time_cur (4) from Dev || id_dev(4) || H(M_SRV_URL)(32) || attest_result(1) || time_attest(4)]
            X509Certificate devCert;
            try {
                MessageDigest digest = MessageDigest.getInstance("SHA-256");
                byte[] hashedShortenedUrl = digest.digest(shortenedUrl.getBytes(StandardCharsets.UTF_8));

                ByteArrayOutputStream baos = new ByteArrayOutputStream();

                baos.write(n_dev, 0, n_dev.length);
                baos.write(time_cur, 0, time_cur.length);
                baos.write(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(devID).array(), 0, 4);
                baos.write(hashedShortenedUrl, 0, 32);
                baos.write(attest_result, 0, attest_result.length);
                baos.write(time_attest, 0, time_attest.length);

                devCert = (X509Certificate) CertificateFactory.getInstance("X.509")
                        .generateCertificate(new ByteArrayInputStream(devCertString.getBytes(StandardCharsets.UTF_8)));
                PublicKey devPublicKey = devCert.getPublicKey();
                KeyFactory keyFactory = KeyFactory.getInstance("EC");
                X509EncodedKeySpec devPublicKeySpec = new X509EncodedKeySpec(devPublicKey.getEncoded());
                PublicKey signaturePublicKey = keyFactory.generatePublic(devPublicKeySpec);
                Signature signatureVerifier = Signature.getInstance("SHA256withECDSA");
                signatureVerifier.initVerify(signaturePublicKey);
                signatureVerifier.update(baos.toByteArray());
                boolean isSignatureValid = signatureVerifier.verify(sig);
                boolean attResultBool = attest_result[0] == 0? true: false;

                Log.d(TAG, "[2]" + (System.currentTimeMillis() - startTime));

                sb.append("Timestamp: " + formattedTs + "\n");
                sb.append("Announce Verification: " + (isSignatureValid==true?"PASS":"FAIL") + "\n");
                sb.append("Attestation Result: " + (attResultBool==true?"PASS":"FAIL") + " \n\t\t\t\t\t@ " + formattedAttTs + "\n\n");
                sb.append("ID: " + dict.get("device_id")+"\n");
                sb.append("Type: " + dict.get("device_type")+"\n");
                sb.append("Manufacturer: " + dict.get("manufacturer")+"\n");
                sb.append("Status: " + dict.get("device_status")+"\n");
//                sb.append("Sensors: " + dict.get("sensors")+"\n");
                sb.append("Sensors: -\n");
                sb.append("Actuators: " + dict.get("actuators")+"\n");
//                sb.append("Location: " + dict.get("location")+"\n");
                sb.append("Network: " + dict.get("network")+"\n");
                sb.append("Description: " + dict.get("description")+"\n");
//                Log.d(TAG, "Verification Result: " + isSignatureValid);
//                Log.d(TAG, "Attestation Result: " + attResultBool + " @ " + formattedDate);
            } catch (CertificateException | InvalidKeySpecException | NoSuchAlgorithmException |
                    SignatureException | InvalidKeyException e) {
                throw new RuntimeException(e);
            }

            textViewScanResults.setText(sb.toString());
        }
    }

    private boolean verifyTime(int ts) {
        final int TIME_LIMIT = 300;
        int currentTime = (int) (System.currentTimeMillis() / 1000);

//        Log.d(TAG, "TS: " + ts);
//        Log.d(TAG, "current Time: " + currentTime);

//        if (ts > currentTime || ts+TIME_LIMIT < currentTime) {
//            return false;
//        }

        return true;
    }

    private String parseUrl (byte[] msg) {
        final int urlTLenOffset = msg.length-1-4-1;
        int urlTLen = Byte.toUnsignedInt(msg[urlTLenOffset]);
        byte[] urlByte = Arrays.copyOfRange(msg, urlTLenOffset-urlTLen, urlTLenOffset);
        String urlString = new String(urlByte, StandardCharsets.UTF_8);
        return urlString;
    }

    private int searchTagInBinaryArray(byte[] array) {
        byte[] tag = {(byte)0xDD, 0x00, 0x00, 0x00};
        for (int i = 0; i < array.length - tag.length + 1; i++) {
            boolean match = true;
            for (int j = 0; j < tag.length; j++) {
                if (array[i+j] != tag[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return i;
            }
        }
        return -1;
    }

    private void showNetworksDetails(List<ScanResult> results) {
        final int VENDOR_SPECIFIC_TAG = 0xDD;
        final String PAISA_SSID = "PAISA";

        this.textViewScanResults.setText("");
        StringBuilder sb = new StringBuilder();
        sb.append("Result Count: " + results.size());

        for (int i = 0; i < results.size(); i++) {
            byte[] msg = {0};
            ScanResult result = results.get(i);
            sb.append("\n\n  --------- Network " + i + "/" + results.size() + " ---------");

            sb.append("\n result.capabilities: " + result.capabilities);
            sb.append("\n result.SSID: " + result.SSID); // Network Name.

            if (result.SSID.compareTo(PAISA_SSID) != 0) {
                continue;
            }

//            Log.d(TAG, "SSID: " + result.SSID);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                List<ScanResult.InformationElement> infoElements = result.getInformationElements();
                if (infoElements != null) {
                    for (ScanResult.InformationElement infoElement : infoElements) {
                        // Check if this is a vendor-specific element
                        if (infoElement.getId() == VENDOR_SPECIFIC_TAG) {
                            ByteBuffer bf = infoElement.getBytes();

                            byte[] byteIE = new byte[bf.capacity()];
                            bf.get(byteIE);
                            if (byteIE.length >= 4) {
                                // Parse the vendor-specific data here
                                msg = Arrays.copyOfRange(byteIE, 4, byteIE.length);
                            }
                        }
                    }
                }
            } else {
                Parcel parcel = Parcel.obtain();
                result.writeToParcel(parcel, 0);
                byte[] bytes = parcel.marshall();

                if (!new String(bytes, StandardCharsets.UTF_8).contains(PAISA_SSID)) {
                    continue;
                }


                int offset = searchTagInBinaryArray(bytes);
                int len = ByteBuffer.wrap(Arrays.copyOfRange(bytes, offset+4, offset+8)).order(ByteOrder.LITTLE_ENDIAN).getInt();
                msg = Arrays.copyOfRange(bytes, offset+16, offset+16+len-4);
            }


            String url = parseUrl(msg);

            String n_dev = Base64.encodeToString(
                    Arrays.copyOfRange(msg, 0, 32), Base64.DEFAULT);
            int time_current_int = ByteBuffer.wrap(Arrays.copyOfRange(msg, 32, 36)).order(ByteOrder.LITTLE_ENDIAN).getInt();
            String time_current = Base64.encodeToString(
                    Arrays.copyOfRange(msg, 32, 36), Base64.DEFAULT);
            String sig = Base64.encodeToString(
                    Arrays.copyOfRange(msg, 36, msg.length-url.length()-1-4-1), Base64.DEFAULT);
            String attest_result = Base64.encodeToString(
                    Arrays.copyOfRange(msg, msg.length-4-1, msg.length-4), Base64.DEFAULT);
            String time_attest = Base64.encodeToString(
                    Arrays.copyOfRange(msg, msg.length-4, msg.length), Base64.DEFAULT);


            if (verifyTime(time_current_int) == true) {
                DeviceProfile deviceProfile = new DeviceProfile();
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
                    deviceProfile.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR,
                            url, n_dev, String.valueOf(time_current), sig, attest_result, time_attest);
                } else {
                    deviceProfile.execute(url, n_dev, String.valueOf(time_current), sig, attest_result, time_attest);
                }
            } else {
                Log.d(TAG, "Failure: Incorrect timestamp");
                this.textViewScanResults.setText("Failure: Incorrect timestamp");
            }


        }
//        this.textViewScanResults.setText(sb.toString());
    }
}
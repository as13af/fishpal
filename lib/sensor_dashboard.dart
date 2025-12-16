import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:liquid_progress_indicator_v2/liquid_progress_indicator.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await Firebase.initializeApp(
    options: const FirebaseOptions(
      apiKey: "ISI_API_KEY_ANDAg",
      appId: "ISI_APP_ID_ANDA",
      messagingSenderId: "ISI_SENDER_ID",
      projectId: "fishpal-57e60",
      databaseURL: "https://fishpal-57e60-default-rtdb.firebaseio.com",
    ),
  );

  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Smart Aquarium',
      theme: ThemeData(
        primarySwatch: Colors.teal,
        scaffoldBackgroundColor: Colors.grey[100],
      ),
      home: const SensorDashboard(),
    );
  }
}

class SensorDashboard extends StatefulWidget {
  const SensorDashboard({super.key});
  @override
  State<SensorDashboard> createState() => _SensorDashboardState();
}

class _SensorDashboardState extends State<SensorDashboard> {
  /// Reference ke Realtime Database
  final DatabaseReference dataRef =
  FirebaseDatabase.instance.ref("smart_aquarium/data");
  final DatabaseReference controlRef =
  FirebaseDatabase.instance.ref("smart_aquarium/control");

  // --- Nilai sensor ---
  double ph = 0.0;
  double turbidity = 0.0;
  double temperature = 0.0;
  double waterLevel = 0.0;
  String wifiStatus = "Disconnected";
  String streamUrl = ""; // ESP32-CAM stream URL from Firebase
  String streamError = "";

  // --- Status kontrol ---
  int servoState = 0;
  int servoAngle = 0;
  int pumpState = 0;

  @override
  void initState() {
    super.initState();

    // Listener data sensor
    dataRef.onValue.listen((event) {
      final v = event.snapshot.value;
      if (v is Map) {
        final d = Map<String, dynamic>.from(v);
        setState(() {
          ph = double.tryParse(d['ph']?.toString() ?? '') ?? 0.0;
          turbidity = double.tryParse(d['turbidity']?.toString() ?? '') ?? 0.0;
          temperature =
              double.tryParse(d['temperature']?.toString() ?? '') ?? 0.0;
          waterLevel =
              double.tryParse(d['water_level']?.toString() ?? '') ?? 0.0;
          wifiStatus = d['wifi_status']?.toString() ?? "Disconnected";
        });
      }
    });

    // Listener kontrol -> agar UI ikut berubah jika ESP32 ubah state
    controlRef.onValue.listen((event) {
      final v = event.snapshot.value;
      if (v is Map) {
        final c = Map<String, dynamic>.from(v);
        setState(() {
          servoState = int.tryParse(c['servo_state']?.toString() ?? '') ?? 0;
          servoAngle = int.tryParse(c['servo_angle']?.toString() ?? '') ?? 0;
          pumpState = int.tryParse(c['pump_state']?.toString() ?? '') ?? 0;
          streamUrl = c['esp32_cam_url']?.toString() ?? streamUrl;
          streamError = c['esp32_cam_url'] == null
              ? "Stream URL not set in Firebase"
              : "";
        });
      }
    });
  }

  /// --- Kirim perintah ke Firebase (dibaca oleh ESP32) ---
  Future<void> setServo(int state) async {
    await controlRef.update({
      'servo_state': state,
      'servo_angle': state == 1 ? 90 : 0,
    });
  }

  Future<void> setPump(int state) async {
    await controlRef.update({
      'pump_state': state,
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Colors.teal,
        title: const Text('Smart Aquarium'),
        actions: [
          Icon(
            wifiStatus == "Connected" ? Icons.wifi : Icons.wifi_off,
            color: wifiStatus == "Connected" ? Colors.white : Colors.redAccent,
          ),
          const SizedBox(width: 12),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            _buildWaterLevel(),
            const SizedBox(height: 20),
            _buildSensorGrid(),
            const SizedBox(height: 30),
            _buildControlCard(
              title: "Pakan (Servo)",
              icon: Icons.settings,
              color: Colors.green,
              isActive: servoState == 1,
              onToggle: (v) => setServo(v ? 1 : 0),
              activeText: "Sedang memberi pakan",
              inactiveText: "Tidak memberi pakan",
            ),
            const SizedBox(height: 20),
            _buildControlCard(
              title: "Nutrisi (Pompa)",
              icon: Icons.water,
              color: Colors.blue,
              isActive: pumpState == 1,
              onToggle: (v) => setPump(v ? 1 : 0),
              activeText: "Sedang memberi nutrisi",
              inactiveText: "Tidak memberi nutrisi",
            ),
            const SizedBox(height: 20),
            _buildStreamCard(context),
          ],
        ),
      ),
    );
  }

  // ===================== Widget UI =====================
  Widget _buildWaterLevel() {
    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(28),
        boxShadow: const [
          BoxShadow(color: Colors.black12, blurRadius: 12, offset: Offset(0, 6))
        ],
      ),
      child: Column(
        children: [
          Text("Water Level",
              style: TextStyle(
                  fontSize: 20,
                  fontWeight: FontWeight.bold,
                  color: Colors.teal[800])),
          const SizedBox(height: 20),
          ClipRRect(
            borderRadius: BorderRadius.circular(20),
            child: LiquidLinearProgressIndicator(
              value: (waterLevel / 100).clamp(0.0, 1.0),
              valueColor: const AlwaysStoppedAnimation<Color>(Colors.blue),
              backgroundColor: Colors.grey[200]!,
              direction: Axis.horizontal,
              center: Text("${waterLevel.toStringAsFixed(1)} %",
                  style: const TextStyle(
                      color: Colors.black,
                      fontSize: 18,
                      fontWeight: FontWeight.bold)),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSensorGrid() {
    return GridView.count(
      crossAxisCount: 2,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      mainAxisSpacing: 18,
      crossAxisSpacing: 18,
      childAspectRatio: 1,
      children: [
        _buildSensorCard(Icons.science, "pH",
            ph.toStringAsFixed(2), Colors.blueAccent),
        _buildSensorCard(Icons.opacity, "Turbidity",
            turbidity.toStringAsFixed(1), Colors.indigo),
        _buildSensorCard(Icons.thermostat, "Temperature",
            "${temperature.toStringAsFixed(1)} °C", Colors.orange),
        _buildSensorCard(Icons.rotate_right, "Servo Angle",
            "$servoAngle°", Colors.purple),
      ],
    );
  }

  Widget _buildSensorCard(
      IconData icon, String title, String value, Color color) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(22),
        boxShadow: const [
          BoxShadow(color: Colors.black12, blurRadius: 8, offset: Offset(0, 4))
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            CircleAvatar(
              backgroundColor: color.withOpacity(0.15),
              radius: 28,
              child: Icon(icon, size: 30, color: color),
            ),
            const SizedBox(height: 14),
            Text(title,
                style: TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.w600,
                    color: Colors.grey[800])),
            const SizedBox(height: 8),
            Text(value,
                style: TextStyle(
                    fontSize: 20,
                    fontWeight: FontWeight.bold,
                    color: color)),
          ],
        ),
      ),
    );
  }

  Widget _buildControlCard({
    required String title,
    required IconData icon,
    required Color color,
    required bool isActive,
    required Function(bool) onToggle,
    required String activeText,
    required String inactiveText,
  }) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(22),
        boxShadow: const [
          BoxShadow(color: Colors.black12, blurRadius: 8, offset: Offset(0, 4))
        ],
      ),
      child: Column(
        children: [
          Row(
            children: [
              CircleAvatar(
                backgroundColor: color.withOpacity(0.15),
                radius: 28,
                child: Icon(icon, size: 30, color: color),
              ),
              const SizedBox(width: 16),
              Expanded(
                child: Text(title,
                    style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                        color: Colors.grey[800])),
              ),
              Switch(
                value: isActive,
                activeColor: color,
                onChanged: onToggle,
              ),
            ],
          ),
          const SizedBox(height: 8),
          Text(isActive ? activeText : inactiveText,
              style: TextStyle(
                  color: isActive ? color : Colors.grey[600],
                  fontWeight: FontWeight.w600)),
        ],
      ),
    );
  }

  Widget _buildStreamCard(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(22),
        boxShadow: const [
          BoxShadow(color: Colors.black12, blurRadius: 8, offset: Offset(0, 4))
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              CircleAvatar(
                backgroundColor: Colors.red.withOpacity(0.12),
                radius: 28,
                child: const Icon(Icons.videocam, size: 30, color: Colors.red),
              ),
              const SizedBox(width: 16),
              const Expanded(
                child: Text(
                  "Live Stream",
                  style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                    color: Colors.black87,
                  ),
                ),
              ),
              ElevatedButton.icon(
                icon: const Icon(Icons.play_arrow),
                label: const Text("Lihat"),
                onPressed: () {
                  if (streamUrl.isEmpty) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(
                        content: Text(
                          streamError.isNotEmpty
                              ? streamError
                              : "Stream URL belum tersedia",
                        ),
                      ),
                    );
                    return;
                  }
                  Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (_) => CameraStreamPage(streamUrl: streamUrl),
                    ),
                  );
                },
              ),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            streamUrl.isNotEmpty
                ? "URL: $streamUrl"
                : (streamError.isNotEmpty
                    ? streamError
                    : "Menunggu URL dari Firebase"),
            style: TextStyle(
              color: streamUrl.isNotEmpty ? Colors.teal : Colors.grey[700],
            ),
          ),
        ],
      ),
    );
  }
}

class CameraStreamPage extends StatelessWidget {
  final String streamUrl;
  const CameraStreamPage({super.key, required this.streamUrl});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('ESP32-CAM Stream'),
        backgroundColor: Colors.teal,
      ),
      body: Center(
        child: Image.network(
          streamUrl,
          gaplessPlayback: true, // Keep MJPEG smooth when frames change
          errorBuilder: (_, __, ___) => const Text('Stream tidak tersedia'),
        ),
      ),
    );
  }
}

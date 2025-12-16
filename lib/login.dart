import 'package:flutter/material.dart';
import 'sensor_dashboard.dart';

class LoginPage extends StatefulWidget {
	const LoginPage({super.key});

	@override
	State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
	final TextEditingController _userController = TextEditingController(text: 'admin');
	final TextEditingController _passController = TextEditingController(text: 'admin');
	String? error;

	void _tryLogin() {
		final user = _userController.text.trim();
		final pass = _passController.text.trim();
		if (user == 'admin' && pass == 'admin') {
			setState(() => error = null);
			Navigator.of(context).pushReplacement(
				MaterialPageRoute(builder: (_) => const SensorDashboard()),
			);
		} else {
			setState(() => error = 'Username atau password salah');
		}
	}

	@override
	Widget build(BuildContext context) {
		return Scaffold(
			appBar: AppBar(title: const Text('Login')),
			body: Padding(
				padding: const EdgeInsets.all(24),
				child: Column(
					crossAxisAlignment: CrossAxisAlignment.start,
					children: [
						const Text(
							'Smart Aquarium',
							style: TextStyle(
								fontSize: 24,
								fontWeight: FontWeight.bold,
								color: Color(0xFF0F172A),
							),
						),
						const SizedBox(height: 24),
						TextField(
							controller: _userController,
							decoration: const InputDecoration(
								labelText: 'Username',
								hintText: 'admin',
								border: OutlineInputBorder(),
							),
						),
						const SizedBox(height: 16),
						TextField(
							controller: _passController,
							obscureText: true,
							decoration: const InputDecoration(
								labelText: 'Password',
								hintText: 'admin',
								border: OutlineInputBorder(),
							),
						),
						const SizedBox(height: 16),
						if (error != null)
							Text(
								error!,
								style: const TextStyle(color: Color(0xFFF59E0B)),
							),
						const Spacer(),
						SizedBox(
							width: double.infinity,
							child: ElevatedButton(
								onPressed: _tryLogin,
								child: const Text('Login'),
							),
						),
					],
				),
			),
		);
	}
}

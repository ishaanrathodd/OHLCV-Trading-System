#!/usr/bin/env python3
"""
BSE Trading System Manager
Replaces app_demo.sh and data_fetch_demo.sh with better process management
"""

import subprocess
import sys
import time
import signal
import os
import yaml
import requests
import json
from pathlib import Path
from typing import Optional, Dict, Any

class TradingSystemManager:
    def __init__(self, config_path: str = "config.yaml"):
        self.config = self._load_config(config_path)
        self.processes = {}
        self.running = True
        
        # Setup signal handlers for graceful shutdown
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)

    def _load_config(self, config_path: str) -> Dict[str, Any]:
        """Load configuration from YAML file"""
        try:
            with open(config_path, 'r') as f:
                return yaml.safe_load(f)
        except FileNotFoundError:
            print(f"❌ Config file {config_path} not found!")
            sys.exit(1)
        except yaml.YAMLError as e:
            print(f"❌ Error parsing config file: {e}")
            sys.exit(1)

    def _signal_handler(self, signum, frame):
        """Handle shutdown signals"""
        print(f"\n🛑 Shutdown requested (signal {signum})")
        self.running = False
        self.stop_all_processes()

    def check_questdb_installed(self) -> bool:
        """Check if QuestDB is installed (likely via brew)"""
        try:
            result = subprocess.run(['which', 'questdb'], capture_output=True, text=True)
            if result.returncode == 0:
                return True
            
            # Check brew services
            result = subprocess.run(['brew', 'list', 'questdb'], capture_output=True, text=True)
            return result.returncode == 0
        except Exception:
            return False

    def start_questdb(self) -> bool:
        """Start QuestDB service"""
        try:
            print("🔄 Starting QuestDB...")
            
            # Try to start via brew services first
            result = subprocess.run(['brew', 'services', 'start', 'questdb'], 
                                  capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✅ QuestDB started via brew services")
                # Wait for QuestDB to be ready
                return self._wait_for_questdb_ready()
            else:
                print(f"ℹ️  Brew services failed: {result.stderr.strip()}")
                
                # Try direct questdb command
                print("🔄 Trying direct QuestDB startup...")
                process = subprocess.Popen(['questdb', 'start'], 
                                         stdout=subprocess.PIPE, 
                                         stderr=subprocess.PIPE)
                self.processes['questdb'] = process
                
                # Wait for QuestDB to be ready
                return self._wait_for_questdb_ready()
                
        except Exception as e:
            print(f"❌ Failed to start QuestDB: {e}")
            return False

    def _wait_for_questdb_ready(self, timeout: int = 60) -> bool:
        """Wait for QuestDB to be ready to accept connections"""
        print("⏳ Waiting for QuestDB to be ready...")
        
        for i in range(timeout):
            if self.check_questdb_running():
                print("✅ QuestDB is ready!")
                return True
            time.sleep(1)
            if i % 10 == 9:  # Print every 10 seconds
                print(f"   Still waiting... ({i+1}s)")
        
        print("❌ QuestDB failed to start within timeout")
        return False

    def stop_questdb(self):
        """Stop QuestDB service if we started it"""
        if 'questdb' in self.processes:
            print("🛑 Stopping QuestDB...")
            process = self.processes['questdb']
            if process and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                    print("✅ QuestDB stopped")
                except subprocess.TimeoutExpired:
                    print("⚠️  Force killing QuestDB")
                    process.kill()
        else:
            # Try to stop via brew services (in case it was started that way)
            try:
                subprocess.run(['brew', 'services', 'stop', 'questdb'], 
                             capture_output=True, text=True)
            except Exception:
                pass

    def check_questdb_running(self) -> bool:
        """Check if QuestDB is running"""
        try:
            # Check if process is running
            result = subprocess.run(['pgrep', '-f', 'questdb'], 
                                  capture_output=True, text=True)
            if result.returncode != 0:
                return False
                
            # Check if ILP port is open (more reliable than HTTP)
            import socket
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2)
            result = sock.connect_ex((self.config['questdb']['host'], self.config['questdb']['port']))
            sock.close()
            
            if result == 0:
                return True
                
            # Also check PostgreSQL port
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2)
            result = sock.connect_ex((self.config['questdb']['host'], self.config['questdb']['postgres_port']))
            sock.close()
            
            return result == 0
            
        except Exception:
            return False

    def check_pipeline_running(self) -> Optional[int]:
        """Check if data pipeline is running, return PID if found"""
        try:
            result = subprocess.run(['pgrep', '-f', self.config['app']['pipeline_executable']], 
                                  capture_output=True, text=True)
            if result.returncode == 0 and result.stdout.strip():
                return int(result.stdout.strip().split('\n')[0])
        except Exception:
            pass
        return None

    def build_project(self):
        """Build the C++ project"""
        print("🔨 Building project...")
        build_dir = self.config['app']['build_dir']
        build_type = self.config['build']['type']
        
        try:
            subprocess.run(['cmake', '--build', build_dir, '--config', build_type], 
                         check=True, cwd='.')
            print("✅ Build completed successfully")
        except subprocess.CalledProcessError as e:
            print(f"❌ Build failed: {e}")
            sys.exit(1)

    def start_pipeline(self):
        """Start the Alpha Vantage pipeline"""
        pipeline_pid = self.check_pipeline_running()
        if pipeline_pid:
            print(f"✅ Data pipeline is already running (PID: {pipeline_pid})")
            return pipeline_pid

        print("🔄 Starting Alpha Vantage → QuestDB data pipeline...")
        
        # Build command with ALL config values (no hardcoded values in C++)
        cmd = [
            f"./{self.config['app']['build_dir']}/{self.config['app']['pipeline_executable']}",
            self.config['alpha_vantage']['api_key'],
            self.config['questdb']['host'],
            str(self.config['questdb']['port']),
            str(self.config['questdb']['batch_size']),
            str(self.config['alpha_vantage']['polling_interval_seconds']),
        ]
        
        # Add symbols as additional arguments
        cmd.extend(self.config['alpha_vantage']['symbols'])
        
        # Start process
        log_file = open(self.config['app']['log_file'], 'w')
        process = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT)
        
        self.processes['pipeline'] = process
        print(f"✅ Data pipeline started (PID: {process.pid})")
        print(f"📝 Logs: {self.config['app']['log_file']}")
        
        # Give it time to initialize
        time.sleep(3)
        return process.pid

    def start_gui(self):
        """Start the Qt GUI application"""
        print("🖥️  Launching Qt GUI Application...")
        print("   - Real-time BSE market data dashboard")
        print("   - Interactive charts for RELIANCE & TCS")
        print("   - Live data updates every 5 seconds")
        
        cmd = [
            f"./{self.config['app']['build_dir']}/{self.config['app']['gui_executable']}",
            self.config['questdb']['host'],
            str(self.config['questdb']['postgres_port']),
            self.config['alpha_vantage']['api_key']
        ]
        
        print(f"🔧 Starting GUI with command: {' '.join(cmd)}")
        
        try:
            # Start GUI with error capture
            process = subprocess.Popen(cmd, 
                                     stdout=subprocess.PIPE, 
                                     stderr=subprocess.PIPE)
            self.processes['gui'] = process
            
            # Give GUI a moment to start
            time.sleep(2)
            
            # Check if GUI started successfully
            if process.poll() is None:
                print(f"✅ GUI started successfully (PID: {process.pid})")
                print("📱 Check your screen for the trading dashboard window")
            else:
                # GUI exited immediately, show error
                stdout, stderr = process.communicate()
                print(f"❌ GUI failed to start:")
                if stderr:
                    print(f"   Error: {stderr.decode().strip()}")
                if stdout:
                    print(f"   Output: {stdout.decode().strip()}")
                return None
                
            return process.pid
            
        except Exception as e:
            print(f"❌ Failed to start GUI: {e}")
            return None

    def display_current_data(self):
        """Display current BSE data from QuestDB"""
        questdb_url = f"http://{self.config['questdb']['host']}:{self.config['questdb']['http_port']}"
        
        try:
            # Get total record count
            count_response = requests.get(
                f"{questdb_url}/exec?query=SELECT COUNT(*) FROM trades WHERE exchange = 'BSE'",
                timeout=5
            )
            if count_response.status_code == 200:
                count_data = count_response.json()
                total_records = count_data.get('dataset', [[0]])[0][0]
                print(f"📊 Current BSE data in QuestDB:")
                print(f"   Total BSE records: {total_records}")
            
            # Get latest prices
            prices_response = requests.get(
                f"{questdb_url}/exec?query=SELECT symbol, price, side, timestamp FROM trades WHERE exchange = 'BSE' ORDER BY timestamp DESC LIMIT 5",
                timeout=5
            )
            if prices_response.status_code == 200:
                prices_data = prices_response.json()
                dataset = prices_data.get('dataset', [])
                if dataset:
                    print("💼 Latest BSE prices:")
                    for row in dataset:
                        symbol, price, side, timestamp = row
                        print(f"   {symbol}: ₹{price} ({side}) at {timestamp}")
                else:
                    print("   Loading data...")
                    
        except Exception as e:
            print(f"   Error fetching data: {e}")

    def stop_all_processes(self):
        """Stop all managed processes including QuestDB if we started it"""
        for name, process in self.processes.items():
            if name == 'questdb':
                continue  # Handle QuestDB separately
            if process and process.poll() is None:
                print(f"🛑 Stopping {name} (PID: {process.pid})")
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    print(f"⚠️  Force killing {name}")
                    process.kill()
        
        # Stop QuestDB last
        self.stop_questdb()

    def run_complete_demo(self):
        """Run the complete BSE trading system demo"""
        print("🚀 BSE Trading System - Complete Demo")
        print("======================================")
        print()

        # Check and start QuestDB if needed
        if not self.check_questdb_running():
            if not self.check_questdb_installed():
                print("❌ QuestDB is not installed!")
                print("Please install QuestDB first: brew install questdb")
                sys.exit(1)
            
            if not self.start_questdb():
                print("❌ Failed to start QuestDB!")
                sys.exit(1)
        else:
            print("✅ QuestDB is already running")

        # Build project
        self.build_project()

        # Start pipeline
        self.start_pipeline()

        # Display current data
        self.display_current_data()
        print()

        # Start GUI
        self.start_gui()

        # Keep running and monitoring
        try:
            while self.running:
                time.sleep(self.config['monitoring']['health_check_interval'])
                # Basic health check
                for name, process in list(self.processes.items()):
                    if name == 'questdb':
                        continue  # QuestDB managed separately
                    if process.poll() is not None:
                        print(f"⚠️  Process {name} has stopped")
                        
        except KeyboardInterrupt:
            pass
        finally:
            self.stop_all_processes()

    def run_data_only_demo(self):
        """Run data fetching only (replaces data_fetch_demo.sh)"""
        print("=== Alpha Vantage BSE Data Demo ===")
        print(f"API Key: {self.config['alpha_vantage']['api_key'][:8]}...")
        print()

        self.build_project()
        
        print("Available demo modes:")
        print("1. Alpha Vantage BSE Data → QuestDB (Primary)")
        print("2. Alpha Vantage BSE Data Only (Testing)")
        print("3. CSV Test Mode (Development)")
        
        choice = input("Enter choice [1/2/3/q]: ").strip()
        
        if choice == '1':
            # Check and start QuestDB if needed
            if not self.check_questdb_running():
                if not self.check_questdb_installed():
                    print("❌ QuestDB is not installed!")
                    print("Please install QuestDB first: brew install questdb")
                    sys.exit(1)
                
                if not self.start_questdb():
                    print("❌ Failed to start QuestDB!")
                    sys.exit(1)
            else:
                print("✅ QuestDB is already running")
                
            self.start_pipeline()
            
            # Keep pipeline running
            try:
                print("\n🔄 Pipeline running... Press Ctrl+C to stop")
                while self.running:
                    time.sleep(5)
            except KeyboardInterrupt:
                pass
            finally:
                self.stop_all_processes()
            
        elif choice == '2':
            # Data fetching only
            cmd = [f"./{self.config['app']['build_dir']}/src/engine/ingest/ingest_app",
                   self.config['alpha_vantage']['api_key']]
            subprocess.run(cmd)
            
        elif choice == '3':
            # CSV test mode
            cmd = [f"./{self.config['app']['build_dir']}/src/engine/ingest/ingest_app",
                   "test_data/sample_ticks.csv", "replay"]
            subprocess.run(cmd)
            
        elif choice.lower() == 'q':
            sys.exit(0)
        else:
            print("Invalid choice")
            sys.exit(1)


def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 run_trading_system.py demo     # Complete system demo")
        print("  python3 run_trading_system.py data     # Data fetching only")
        sys.exit(1)
    
    manager = TradingSystemManager()
    
    if sys.argv[1] == 'demo':
        manager.run_complete_demo()
    elif sys.argv[1] == 'data':
        manager.run_data_only_demo()
    else:
        print(f"Unknown command: {sys.argv[1]}")
        sys.exit(1)


if __name__ == "__main__":
    main()

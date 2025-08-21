#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include "defines.h"
#include "state.h"

// External declaration for the web server object
extern ESP8266WebServer server;

// Function declarations for RESTful API endpoints
String getSignalQuality(int rssi);     // Converts RSSI to signal quality string
void handleStatus();                   // Returns device status and statistics
void handleWriteRFID();               // Sets new RFID UID
void handleReset();                   // Handles reset request
void handleRelay();                   // Handles relay control requests
void handleSetDieID();                // Sets new die ID
void handleSetPersonnelID();          // Sets new personnel ID
void handleRoot();                    // Serves the Vue web interface
void setupWebServer();                // Initializes the web server

// HTML content for the enhanced Vue-based web interface
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hydraulic Press Control</title>
  <script src="https://cdn.jsdelivr.net/npm/vue@3.2.47/dist/vue.global.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/axios@1.6.8/dist/axios.min.js"></script>
  <script src="https://cdn.tailwindcss.com"></script>
  <style>
    .toast {
      position: fixed;
      bottom: 20px;
      right: 20px;
      padding: 12px 24px;
      border-radius: 4px;
      color: white;
      z-index: 1000;
      transition: opacity 0.3s ease-in-out;
    }
    .toast-success { background-color: #10b981; }
    .toast-error { background-color: #ef4444; }
  </style>
</head>
<body class="bg-gray-100 font-sans">
  <div id="app" class="max-w-4xl mx-auto p-6">
    <h1 class="text-3xl font-bold text-center text-gray-800 mb-8">Hydraulic Press Control</h1>

    <!-- Toast Notification -->
    <div v-if="toastMessage" :class="['toast', toastType === 'success' ? 'toast-success' : 'toast-error']">
      {{ toastMessage }}
    </div>

    <!-- Device Status Section -->
    <div class="bg-white rounded-lg shadow-md mb-6">
      <button @click="toggleSection('status')" class="w-full text-left p-4 bg-gray-50 rounded-t-lg flex justify-between items-center">
        <h2 class="text-xl font-semibold text-gray-700">Device Status</h2>
        <span class="text-gray-500">{{ statusSectionOpen ? '▲' : '▼' }}</span>
      </button>
      <div v-if="statusSectionOpen" class="p-4">
        <div v-if="loadingStatus" class="text-center text-gray-500">Loading...</div>
        <div v-else class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Device</label>
            <p class="text-gray-800">{{ status.device }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">WiFi Signal</label>
            <p class="text-gray-800">{{ status.wifi_signal }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">IP Address</label>
            <p class="text-gray-800">{{ status.ip_address }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Relay Status</label>
            <p class="text-gray-800">{{ status.relay_status }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Unsent Count</label>
            <p class="text-gray-800">{{ status.unsent_count }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Failed Count</label>
            <p class="text-gray-800">{{ status.failed_count }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Last Pulse Duration</label>
            <p class="text-gray-800">{{ status.last_pulse_duration }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Last Send Duration</label>
            <p class="text-gray-800">{{ status.last_send_duration }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Max Send Duration</label>
            <p class="text-gray-800">{{ status.max_send_duration }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Last Short Pulse Duration</label>
            <p class="text-gray-800">{{ status.last_short_pulse_duration }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Min Pulse Duration</label>
            <p class="text-gray-800">{{ status.min_pulse_duration }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Last Edge Time</label>
            <p class="text-gray-800">{{ status.last_edge_time }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Pulse Train Start</label>
            <p class="text-gray-800">{{ status.pulse_train_start }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Last Send Attempt</label>
            <p class="text-gray-800">{{ status.last_send_attempt }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">In Pulse Train</label>
            <p class="text-gray-800">{{ status.in_pulse_train }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Should Send</label>
            <p class="text-gray-800">{{ status.should_send }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Device Info Sent</label>
            <p class="text-gray-800">{{ status.device_info_sent }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Request In Progress</label>
            <p class="text-gray-800">{{ status.request_in_progress }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Is Device Info Request</label>
            <p class="text-gray-800">{{ status.is_device_info_request }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Request Start Time</label>
            <p class="text-gray-800">{{ status.request_start_time }} ms</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Is RFID Request</label>
            <p class="text-gray-800">{{ status.is_rfid_request }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">RFID Status</label>
            <p class="text-gray-800">{{ status.rfid_status }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Last UID</label>
            <p class="text-gray-800">{{ status.last_uid }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Last Tag Data</label>
            <p class="text-gray-800">{{ status.last_tag_data }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">New Tag Data</label>
            <p class="text-gray-800">{{ status.new_tag_data }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Current Die ID</label>
            <p class="text-gray-800">{{ status.current_die_id }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Attempt Write Tag</label>
            <p class="text-gray-800">{{ status.attempt_write_tag }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Opto Pin Status</label>
            <p class="text-gray-800">{{ status.opto_pin_status }}</p>
          </div>
          <div class="p-4 bg-gray-50 rounded-md">
            <label class="block text-sm font-medium text-gray-600">Last Tag Read Time</label>
            <p class="text-gray-800">{{ status.last_tag_read_time }} ms</p>
          </div>
        </div>
        <div class="mt-4 flex items-center space-x-4">
          <button @click="fetchStatus" class="bg-blue-600 text-white px-4 py-2 rounded-md hover:bg-blue-700">Refresh Status</button>
          <label class="flex items-center space-x-2">
            <input type="checkbox" v-model="autoRefresh" class="h-4 w-4 text-blue-600">
            <span class="text-gray-600">Auto-refresh every 5s</span>
          </label>
        </div>
      </div>
    </div>

    <!-- Relay Control Section -->
    <div class="bg-white rounded-lg shadow-md mb-6">
      <button @click="toggleSection('relay')" class="w-full text-left p-4 bg-gray-50 rounded-t-lg flex justify-between items-center">
        <h2 class="text-xl font-semibold text-gray-700">Relay Control</h2>
        <span class="text-gray-500">{{ relaySectionOpen ? '▲' : '▼' }}</span>
      </button>
      <div v-if="relaySectionOpen" class="p-4">
        <div class="flex space-x-4">
          <button @click="toggleRelay('on')" class="bg-green-600 text-white px-4 py-2 rounded-md hover:bg-green-700" :disabled="loadingRelay">Turn Relay On</button>
          <button @click="toggleRelay('off')" class="bg-red-600 text-white px-4 py-2 rounded-md hover:bg-red-700" :disabled="loadingRelay">Turn Relay Off</button>
        </div>
      </div>
    </div>

    <!-- RFID Write Section -->
    <div class="bg-white rounded-lg shadow-md mb-6">
      <button @click="toggleSection('rfid')" class="w-full text-left p-4 bg-gray-50 rounded-t-lg flex justify-between items-center">
        <h2 class="text-xl font-semibold text-gray-700">Write RFID Tag</h2>
        <span class="text-gray-500">{{ rfidSectionOpen ? '▲' : '▼' }}</span>
      </button>
      <div v-if="rfidSectionOpen" class="p-4">
        <div class="flex space-x-4">
          <select v-model="rfidTagType" class="border border-gray-300 rounded-md p-2">
            <option value="die">Die</option>
            <option value="personnel">Personnel</option>
          </select>
          <input type="text" v-model="rfidTagId" placeholder="Enter Tag ID (alphanumeric)" class="border border-gray-300 rounded-md p-2 flex-1">
          <button @click="writeRFID" class="bg-blue-600 text-white px-4 py-2 rounded-md hover:bg-blue-700" :disabled="loadingRFID || !rfidTagId">Write RFID</button>
        </div>
      </div>
    </div>

    <!-- Set Die ID Section -->
    <div class="bg-white rounded-lg shadow-md mb-6">
      <button @click="toggleSection('dieID')" class="w-full text-left p-4 bg-gray-50 rounded-t-lg flex justify-between items-center">
        <h2 class="text-xl font-semibold text-gray-700">Set Die ID</h2>
        <span class="text-gray-500">{{ dieIDSectionOpen ? '▲' : '▼' }}</span>
      </button>
      <div v-if="dieIDSectionOpen" class="p-4">
        <input type="text" v-model="dieID" placeholder="Enter Die ID (alphanumeric, max 15 chars)" class="border border-gray-300 rounded-md p-2 w-full mb-2" maxlength="15">
        <button @click="setDieID" class="bg-blue-600 text-white px-4 py-2 rounded-md hover:bg-blue-700" :disabled="loadingDieID || !dieID">Set Die ID</button>
      </div>
    </div>

    <!-- Set Personnel ID Section -->
    <div class="bg-white rounded-lg shadow-md mb-6">
      <button @click="toggleSection('personnelID')" class="w-full text-left p-4 bg-gray-50 rounded-t-lg flex justify-between items-center">
        <h2 class="text-xl font-semibold text-gray-700">Set Personnel ID</h2>
        <span class="text-gray-500">{{ personnelIDSectionOpen ? '▲' : '▼' }}</span>
      </button>
      <div v-if="personnelIDSectionOpen" class="p-4">
        <input type="text" v-model="personnelID" placeholder="Enter Personnel ID (alphanumeric)" class="border border-gray-300 rounded-md p-2 w-full mb-2">
        <button @click="setPersonnelID" class="bg-blue-600 text-white px-4 py-2 rounded-md hover:bg-blue-700" :disabled="loadingPersonnelID || !personnelID">Set Personnel ID</button>
      </div>
    </div>

    <!-- Reset EEPROM Section -->
    <div class="bg-white rounded-lg shadow-md mb-6">
      <button @click="toggleSection('reset')" class="w-full text-left p-4 bg-gray-50 rounded-t-lg flex justify-between items-center">
        <h2 class="text-xl font-semibold text-gray-700">Reset EEPROM</h2>
        <span class="text-gray-500">{{ resetSectionOpen ? '▲' : '▼' }}</span>
      </button>
      <div v-if="resetSectionOpen" class="p-4">
        <button @click="resetEEPROM" class="bg-red-600 text-white px-4 py-2 rounded-md hover:bg-red-700" :disabled="loadingReset">Reset EEPROM</button>
      </div>
    </div>

    <!-- Firmware Update Section -->
    <div class="bg-white rounded-lg shadow-md mb-6">
      <button @click="toggleSection('firmware')" class="w-full text-left p-4 bg-gray-50 rounded-t-lg flex justify-between items-center">
        <h2 class="text-xl font-semibold text-gray-700">Firmware Update</h2>
        <span class="text-gray-500">{{ firmwareSectionOpen ? '▲' : '▼' }}</span>
      </button>
      <div v-if="firmwareSectionOpen" class="p-4">
        <input type="file" accept=".bin" @change="selectFirmwareFile" class="border border-gray-300 rounded-md p-2 w-full mb-2">
        <p v-if="firmwareFile" class="text-gray-600 mb-2">Selected: {{ firmwareFile.name }}</p>
        <button @click="uploadFirmware" class="bg-blue-600 text-white px-4 py-2 rounded-md hover:bg-blue-700" :disabled="loadingFirmware || !firmwareFile">Upload Firmware</button>
      </div>
    </div>
  </div>

  <script>
    const { createApp } = Vue;

    createApp({
      data() {
        return {
          status: {
            device: '',
            wifi_signal: '',
            ip_address: '',
            relay_status: '',
            unsent_count: 0,
            failed_count: 0,
            last_pulse_duration: 0,
            last_send_duration: 0,
            max_send_duration: 0,
            last_short_pulse_duration: 0,
            min_pulse_duration: 0,
            last_edge_time: 0,
            pulse_train_start: 0,
            last_send_attempt: 0,
            in_pulse_train: '',
            should_send: '',
            device_info_sent: '',
            request_in_progress: '',
            is_device_info_request: '',
            request_start_time: 0,
            is_rfid_request: '',
            rfid_status: '',
            last_uid: '',
            last_tag_data: '',
            new_tag_data: '',
            current_die_id: '',
            attempt_write_tag: '',
            opto_pin_status: '',
            last_tag_read_time: 0
          },
          rfidTagType: 'die',
          rfidTagId: '',
          dieID: '',
          personnelID: '',
          firmwareFile: null,
          toastMessage: '',
          toastType: '',
          loadingStatus: false,
          loadingRelay: false,
          loadingRFID: false,
          loadingDieID: false,
          loadingPersonnelID: false,
          loadingReset: false,
          loadingFirmware: false,
          statusSectionOpen: true,
          relaySectionOpen: true,
          rfidSectionOpen: true,
          dieIDSectionOpen: true,
          personnelIDSectionOpen: true,
          resetSectionOpen: true,
          firmwareSectionOpen: true,
          autoRefresh: false,
          refreshInterval: null
        };
      },
      methods: {
        async fetchStatus() {
          this.loadingStatus = true;
          try {
            const response = await axios.get('/status');
            this.status = response.data;
          } catch (error) {
            this.showToast('Error fetching status: ' + (error.response?.data?.error || 'Network error'), 'error');
          } finally {
            this.loadingStatus = false;
          }
        },
        async toggleRelay(action) {
          this.loadingRelay = true;
          try {
            const response = await axios.post('/relay', `action=${action}`, {
              headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
            });
            this.showToast(response.data.status, 'success');
            this.fetchStatus();
          } catch (error) {
            this.showToast('Error: ' + (error.response?.data?.error || 'Failed to toggle relay'), 'error');
          } finally {
            this.loadingRelay = false;
          }
        },
        async writeRFID() {
          if (!this.rfidTagId || !/^[a-zA-Z0-9]+$/.test(this.rfidTagId)) {
            this.showToast('Error: Tag ID must be alphanumeric', 'error');
            return;
          }
          const tagData = `${this.rfidTagType}:${this.rfidTagId}`;
          this.loadingRFID = true;
          try {
            const response = await axios.post('/write_rfid', `newTagData=${tagData}`, {
              headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
            });
            this.showToast(`Success: ${response.data.status}, Tag Data: ${response.data.new_Tag_Data}`, 'success');
          } catch (error) {
            this.showToast('Error: ' + (error.response?.data?.error || 'Failed to write RFID'), 'error');
          } finally {
            this.loadingRFID = false;
          }
        },
        async setDieID() {
          if (!this.dieID || !/^[a-zA-Z0-9]+$/.test(this.dieID) || this.dieID.length > 15) {
            this.showToast('Error: Die ID must be alphanumeric and max 15 characters', 'error');
            return;
          }
          this.loadingDieID = true;
          try {
            const response = await axios.post('/set_die_id', `dieID=${this.dieID}`, {
              headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
            });
            this.showToast(`Success: ${response.data.status}, Die ID: ${response.data.die_id}`, 'success');
            this.fetchStatus();
          } catch (error) {
            this.showToast('Error: ' + (error.response?.data?.error || 'Failed to set Die ID'), 'error');
          } finally {
            this.loadingDieID = false;
          }
        },
        async setPersonnelID() {
          if (!this.personnelID || !/^[a-zA-Z0-9]+$/.test(this.personnelID)) {
            this.showToast('Error: Personnel ID must be alphanumeric', 'error');
            return;
          }
          this.loadingPersonnelID = true;
          try {
            const response = await axios.post('/set_personnel_id', `personnelID=${this.personnelID}`, {
              headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
            });
            this.showToast(`Success: ${response.data.status}, Personnel ID: ${response.data.personnel_id}`, 'success');
            this.fetchStatus();
          } catch (error) {
            this.showToast('Error: ' + (error.response?.data?.error || 'Failed to set Personnel ID'), 'error');
          } finally {
            this.loadingPersonnelID = false;
          }
        },
        async resetEEPROM() {
          this.loadingReset = true;
          try {
            const response = await axios.post('/reset');
            this.showToast(response.data.status, 'success');
            this.fetchStatus();
          } catch (error) {
            this.showToast('Error: ' + (error.response?.data?.error || 'Failed to reset EEPROM'), 'error');
          } finally {
            this.loadingReset = false;
          }
        },
        selectFirmwareFile(event) {
          this.firmwareFile = event.target.files[0];
          if (this.firmwareFile && !this.firmwareFile.name.endsWith('.bin')) {
            this.showToast('Error: Please select a .bin file', 'error');
            this.firmwareFile = null;
          }
        },
        async uploadFirmware() {
          if (!this.firmwareFile) {
            this.showToast('Error: No firmware file selected', 'error');
            return;
          }
          this.loadingFirmware = true;
          const formData = new FormData();
          formData.append('update', this.firmwareFile);
          try {
            const response = await axios.post('/update', formData, {
              headers: {
                'Content-Type': 'multipart/form-data',
                'Authorization': 'Basic ' + btoa('admin:password')
              }
            });
            this.showToast('Success: Firmware update initiated', 'success');
          } catch (error) {
            this.showToast('Error: ' + (error.response?.data?.error || 'Failed to upload firmware'), 'error');
          } finally {
            this.loadingFirmware = false;
          }
        },
        showToast(message, type) {
          this.toastMessage = message;
          this.toastType = type;
          setTimeout(() => {
            this.toastMessage = '';
            this.toastType = '';
          }, 3000);
        },
        toggleSection(section) {
          this[`${section}SectionOpen`] = !this[`${section}SectionOpen`];
        },
        startAutoRefresh() {
          if (this.autoRefresh && !this.refreshInterval) {
            this.refreshInterval = setInterval(() => {
              this.fetchStatus();
            }, 5000);
          } else if (!this.autoRefresh && this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
          }
        }
      },
      watch: {
        autoRefresh(newVal) {
          this.startAutoRefresh();
        }
      },
      mounted() {
        this.fetchStatus();
        this.startAutoRefresh();
      },
      beforeUnmount() {
        if (this.refreshInterval) {
          clearInterval(this.refreshInterval);
        }
      }
    }).mount('#app');
  </script>
</body>
</html>
)rawliteral";

#endif
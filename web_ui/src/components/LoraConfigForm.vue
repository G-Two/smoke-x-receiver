<template>
  <div id="lora-config">
    <center>
      <b-overlay
        :show="!dataReceived"
        rounded="sm"
        variant="white"
        opacity="0.85"
        no-fade
      >
        <form id="config-form">
          <label for="frequency"><b>Frequency:</b>
            {{ (selectedFrequency / 1000000).toPrecision(6) }} MHz</label>

          <b-form-input
            id="frequency"
            v-model.number="selectedFrequency"
            type="number"
            class="form-range"
            min="902000000"
            max="928000000"
            step="5000"
          />

          <label for="bandwidth"><b>Bandwidth:</b>
            {{ calcBandwidth(selectedBandwidthIndex) / 1000 }} kHz</label>

          <b-form-input
            id="bandwidth"
            v-model.number="selectedBandwidthIndex"
            type="range"
            class="form-range"
            min="0"
            max="6"
          />

          <label for="spreading-factor"><b>Spreading Factor:</b> {{ selectedSpreadingFactor }}</label>

          <b-form-input
            id="spreading-factor"
            v-model.number="selectedSpreadingFactor"
            type="range"
            class="form-range"
            min="6"
            max="12"
          />

          <label
            class="form-label"
            for="tx-power"
          ><b>Transmit Power:</b> {{ selectedTxPower }}</label>

          <b-form-input
            id="tx-power"
            v-model.number="selectedTxPower"
            type="range"
            class="form-range"
            min="2"
            max="17"
          />

          <label for="coding-rate"><b>Coding Rate:</b> {{ selectedCodingRate }}</label>

          <b-form-input
            id="coding-rate"
            v-model.number="selectedCodingRate"
            type="range"
            class="form-range"
            min="5"
            max="8"
          />

          <label for="preamble-length"><b>Preamble Length:</b> {{ selectedPreambleLength }}</label>

          <b-form-input
            id="preamble-length"
            v-model.number="selectedPreambleLength"
            type="range"
            class="form-range"
            min="6"
            max="65535"
          />

          <label for="sync-word"><b>Sync Word:</b> 0x{{
            (selectedSyncWord * 1).toString(16).toUpperCase()
          }}</label>

          <b-form-input
            id="sync-word"
            v-model.number="selectedSyncWord"
            type="range"
            class="form-range"
            min="0"
            max="255"
          />

          <b-form-checkbox
            v-model="switchImplicitHeader"
            name="implicit-header-switch"
            switch
            size="lg"
          >
            Implicit Header
          </b-form-checkbox>

          <!-- BEGIN Implicit Header Configuration Items -->
          <div
            v-if="switchImplicitHeader"
            id="implicit-header-items"
          >
            <b-form-checkbox
              v-model="switchEnableCRC"
              name="enable-crc-switch"
              switch
              size="lg"
            >
              Enable CRC
            </b-form-checkbox>

            <label for="message-length">Message Length {{ selectedMessageLength }}</label>
            <b-form-input
              id="message-length"
              v-model.number="selectedMessageLength"
              type="range"
              class="form-range"
              min="1"
              max="100"
            />
          </div>

          <!-- END Implicit Header Configuration Items -->
          <center>
            <b-button
              block
              variant="primary"
              @click.prevent="sendToServer"
            >
              Set LoRa Config
            </b-button>
          </center>
        </form>
        <div>
          <h4 for="tx-form">
            Transmit Message
          </h4>
          <form id="tx-form">
            <b-form-input
              v-model="transmitMessage"
              placeholder="Enter message to transmit"
            />
            <b-form-checkbox
              v-model="switchTxRepeatMessage"
              name="tx-repeat-switch"
              switch
              size="lg"
            >
              Repeat Transmission
            </b-form-checkbox>
            <div v-if="switchTxRepeatMessage">
              <label for="tx-interval">Repeat interval (ms)</label>
              <b-form-input
                id="tx-interval"
                v-model.number="selectedTxInterval"
                type="number"
                class="form-range"
                min="0"
                max="4294967295"
              />
            </div>
            <b-row>
              <b-col>
                <center>
                  <b-button
                    block
                    variant="success"
                    @click.prevent="sendCmd('startTx')"
                  >
                    Start Tx
                  </b-button>
                </center>
              </b-col>
              <b-col>
                <center>
                  <b-button
                    block
                    variant="danger"
                    @click.prevent="sendCmd('stopTx')"
                  >
                    Stop Tx
                  </b-button>
                </center>
              </b-col>
            </b-row>
          </form>

          <h4 for="rx-form">
            Receive Messages
          </h4>
          <form id="rx-form">
            <b-row>
              <b-col>
                <center>
                  <b-button
                    block
                    variant="success"
                    @click.prevent="sendCmd('startRx')"
                  >
                    Start Rx
                  </b-button>
                </center>
              </b-col>
              <b-col>
                <center>
                  <b-button
                    block
                    variant="danger"
                    @click.prevent="sendCmd('stopRx')"
                  >
                    Stop Rx
                  </b-button>
                </center>
              </b-col>
            </b-row>
          </form>
        </div>
      </b-overlay>
    </center>
  </div>
</template>

<script>
import * as axios from "axios"

export default {
  name: "LoraConfigForm",
  data() {
    return {
      selectedFrequency: 910500000,
      selectedTxPower: 12,
      selectedBandwidthIndex: 4,
      selectedBandwidth: 125000,
      selectedCodingRate: 5,
      selectedSpreadingFactor: 9,
      selectedMessageLength: 50,
      selectedPreambleLength: 10,
      selectedSyncWord: 0x12,
      switchImplicitHeader: false,
      switchEnableCRC: true,
      receivedMessages: "",
      transmitMessage: "",
      switchTxRepeatMessage: false,
      selectedTxInterval: 5000,
      dataReceived: false,
    }
  },
  mounted: async function () {
    axios
      .get("rf-params")
      .then((res) => {
        console.log(res)
        this.selectedFrequency = res.data.frequency
        this.selectedTxPower = res.data.txPower
        this.selectedBandwidthIndex = this.calcBandwidthIndex(
          res.data.bandwidth
        )
        this.selectedBandwidth = res.data.bandwidth
        this.selectedCodingRate = res.data.codingRate
        this.selectedSpreadingFactor = res.data.spreadingFactor
        this.selectedMessageLength = res.data.messageLength
        this.selectedPreambleLength = res.data.preambleLength
        this.selectedSyncWord = res.data.syncWord
        this.switchImplicitHeader = res.data.implicitHeader
        this.switchEnableCRC = res.data.enableCRC
        this.dataReceived = true
      })
      .catch((error) => {
        console.log(error)
      })
  },
  methods: {
    sendToServer() {
      axios.post("rf-params", {
        frequency: this.selectedFrequency,
        txPower: this.selectedTxPower,
        bandwidth: this.selectedBandwidth,
        spreadingFactor: this.selectedSpreadingFactor,
        codingRate: this.selectedCodingRate,
        implicitHeader: this.switchImplicitHeader,
        enableCRC: this.switchEnableCRC,
        messageLength: this.selectedMessageLength,
        preambleLength: this.selectedPreambleLength,
        syncWord: this.selectedSyncWord,
      })
    },
    sendCmd(cmd) {
      var json = {
        command: cmd,
      }
      if (cmd == "startTx") {
        json.message = this.transmitMessage
        if (this.switchTxRepeatMessage) {
          json.repeatInterval = this.selectedTxInterval
        }
      }
      axios.post("cmd", json)
      console.log(cmd)
    },
    calcBandwidth(input) {
      var retVal = 7812.5
      for (var i = input; i > 0; i--) {
        retVal *= 2
      }
      this.selectedBandwidth = retVal
      return retVal
    },
    calcBandwidthIndex(input) {
      for (var i = 0; input >= 15625; i++) {
        input /= 2
      }
      return i
    },
  },
}
</script>

<style scoped>
label {
  font-weight: bold;
  margin-bottom: 0;
}

button {
  width: 12rem;
}
</style>

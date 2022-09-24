<template>
  <div class="container">
    <b-overlay
      :show="!loaded"
      rounded="sm"
      variant="white"
      opacity="0.15"
      no-fade
    >
      <line-chart
        :chart-data="chartData"
        :options="options"
      />
    </b-overlay>
  </div>
</template>

<script>
import LineChart from "./LineChart.js"
import * as axios from "axios"

export default {
  name: "LineChartContainer",
  components: { LineChart },
  data: () => ({
    timer: null,
    loaded: false,
    chartData: null,
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        xAxes: [
          {
            type: "time",
            time: {
              unit: "minutes",
              displayFormats: { minutes: "HH:mm" },
            },
          },
        ],
      },
    },
  }),
  created: async function () {
    await this.getData()
    this.timer = setInterval(this.getData, 30000)
  },
  beforeDestroy: function () {
    clearInterval(this.timer)
  },
  methods: {
    convertData(data) {
      const date = new Date()
      const now = date.getTime()
      const labels = Array(data.probe_1.history.length)
      for (var i = data.probe_1.history.length - 1; i >= 0; i--) {
        labels[i] = now - i * 30000
      }
      labels.reverse()
      console.log(Object.values(data).length)
      if (Object.values(data).length == 3) {
        return {
          labels: labels,
          datasets: [
            {
              label: "Probe 1",
              data: data.probe_1.history,
              fill: false,
              borderColor: "rgb(200, 75, 75)",
              tension: 0,
              pointRadius: 0,
            },
            {
              label: "Probe 2",
              data: data.probe_2.history,
              fill: false,
              borderColor: "rgb(75, 192, 192)",
              tension: 0,
              pointRadius: 0,
            },
          ],
        }
      } else if (Object.values(data).length == 5) {
        return {
          labels: labels,
          datasets: [
            {
              label: "Probe 1",
              data: data.probe_1.history,
              fill: false,
              borderColor: "rgb(200, 75, 75)",
              tension: 0,
              pointRadius: 0,
            },
            {
              label: "Probe 2",
              data: data.probe_2.history,
              fill: false,
              borderColor: "rgb(75, 192, 192)",
              tension: 0,
              pointRadius: 0,
            },
            {
              label: "Probe 3",
              data: data.probe_3.history,
              fill: false,
              borderColor: "rgb(75, 200, 75)",
              tension: 0,
              pointRadius: 0,
            },
            {
              label: "Probe 4",
              data: data.probe_4.history,
              fill: false,
              borderColor: "rgb(192, 75, 192)",
              tension: 0,
              pointRadius: 0,
            },
          ],
        }
      }
    },
    async getData() {
      axios
        .get("data")
        .then((res) => {
          console.log(res)
          this.chartData = this.convertData(res.data)
          this.loaded = true
        })
        .catch((error) => {
          console.log(error)
        })
    },
  },
}
</script>

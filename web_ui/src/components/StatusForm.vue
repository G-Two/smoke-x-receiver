<template>
  <div id="canvasWrapper" style="position: relative; height: 80vh">
    <Line v-if="loaded" :data="chartData" :options="options" />
    <h3 v-else>
      <br />
      No temperature information received
    </h3>
  </div>
</template>

<script>
import { Line } from "vue-chartjs"
import {
  Chart as ChartJS,
  Title,
  Tooltip,
  Legend,
  LineElement,
  CategoryScale,
  LinearScale,
  PointElement,
} from "chart.js"
import * as axios from "axios"
import { DateTime } from "luxon"

ChartJS.register(
  Title,
  Tooltip,
  Legend,
  LineElement,
  CategoryScale,
  LinearScale,
  PointElement
)

export default {
  name: "LineChart",
  // eslint-disable-next-line
  components: { Line },
  data: () => ({
    loaded: false,
    chartData: null,
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          position: "bottom",
        },
      },
      scales: {
        x: {
          border: {
            display: true,
          },
          grid: {
            display: true,
            drawOnChartArea: true,
            drawTicks: true,
          },
        },
        y: {
          border: {
            display: true,
          },
        },
      },
    },
  }),
  created: async function () {
    await this.getData()
    this.timer = setInterval(this.getData, 30000)
  },
  beforeUnmount: function () {
    clearInterval(this.timer)
  },
  methods: {
    convertData(data) {
      const now = DateTime.now()
      const labels = Array(data.probe_1.history.length)
      for (var i = data.probe_1.history.length - 1; i >= 0; i--) {
        labels[i] = now.minus({ seconds: i * 30 }).toFormat("HH:mm")
      }
      labels.reverse()
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
              pointRadius: 2,
            },
            {
              label: "Probe 2",
              data: data.probe_2.history,
              fill: false,
              borderColor: "rgb(75, 192, 192)",
              tension: 0,
              pointRadius: 2,
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
              pointRadius: 2,
            },
            {
              label: "Probe 2",
              data: data.probe_2.history,
              fill: false,
              borderColor: "rgb(75, 192, 192)",
              tension: 0,
              pointRadius: 2,
            },
            {
              label: "Probe 3",
              data: data.probe_3.history,
              fill: false,
              borderColor: "rgb(75, 200, 75)",
              tension: 0,
              pointRadius: 2,
            },
            {
              label: "Probe 4",
              data: data.probe_4.history,
              fill: false,
              borderColor: "rgb(192, 75, 192)",
              tension: 0,
              pointRadius: 2,
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

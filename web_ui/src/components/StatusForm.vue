<template>
  <div id="status">
    <div v-if="error" class="banner banner-error" role="alert">
      Device unreachable — retrying&hellip;
    </div>

    <template v-if="loaded && probes.length">
      <div class="probe-cards">
        <div
          v-for="p in probes"
          :key="p.key"
          class="probe-card"
          :class="`state-${p.state}`"
          :style="{ '--probe-color': p.color.line }"
        >
          <div class="probe-card-head">
            <span class="probe-dot" />
            <span class="probe-label">{{ p.label }}</span>
            <span class="probe-badge">{{ stateLabel(p.state) }}</span>
          </div>
          <div class="probe-temp">
            {{ fmt(p.current_temp) }}<span class="probe-unit">°F</span>
          </div>
          <div class="probe-range">
            Alarm {{ fmt(p.alarm_min) }}° &ndash; {{ fmt(p.alarm_max) }}°
          </div>
        </div>
      </div>

      <div v-if="billows" class="billows-chip">Billows connected</div>

      <div
        id="canvasWrapper"
        style="position: relative; height: 60vh"
        role="img"
        :aria-label="chartSummary"
      >
        <Line :data="chartData" :options="options" :plugins="[alarmLinesPlugin]" />
      </div>
    </template>

    <h3 v-else-if="loaded">
      <br />
      No temperature information received
    </h3>
    <h3 v-else>
      <br />
      Loading&hellip;
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
import { getJSON } from "../api"
import { isDark } from "../theme"

ChartJS.register(
  Title,
  Tooltip,
  Legend,
  LineElement,
  CategoryScale,
  LinearScale,
  PointElement
)

// The base station transmits every 30 s, so each history sample is 30 s apart.
// The API sends no timestamps, so the x-axis is labelled by relative age
// ("minutes before now") rather than a wall clock we can't honestly claim.
const SAMPLE_SECONDS = 30

// Chart canvas colors per theme (canvas can't read CSS custom properties).
const CHART_COLORS = {
  light: { tick: "#667", grid: "rgba(0, 0, 0, 0.1)", legend: "#2c3e50" },
  dark: { tick: "#99a3b1", grid: "rgba(255, 255, 255, 0.12)", legend: "#e6e8ec" },
}

// Shared palette so the probe cards and chart lines always match. Vivid,
// well-separated hues (Tailwind 500s) that stay sharp on light and dark
// backgrounds; `faint` is the same hue at lower alpha for the min-alarm line.
// `dash`/`point` give each probe a distinct line texture and legend marker so
// the series stay distinguishable without relying on color (colorblind-safe).
const PROBE_COLORS = [
  { line: "#ef4444", faint: "rgba(239, 68, 68, 0.5)", dash: [], point: "circle" }, // red
  { line: "#3b82f6", faint: "rgba(59, 130, 246, 0.5)", dash: [7, 3], point: "rect" }, // blue
  { line: "#22c55e", faint: "rgba(34, 197, 94, 0.5)", dash: [2, 3], point: "triangle" }, // green
  { line: "#f59e0b", faint: "rgba(245, 158, 11, 0.5)", dash: [9, 3, 2, 3], point: "rectRot" }, // amber
]

const probeKeys = (data) =>
  Object.keys(data).filter((k) => /^probe_\d+$/.test(k))

// Inline plugin: draw each probe's alarm min/max as dashed threshold lines.
// Reads alarm data from chart.options.plugins.alarmLines so it stays in sync
// with the reactive options object. No external dependency required.
const alarmLinesPlugin = {
  id: "alarmLines",
  afterDatasetsDraw(chart) {
    const cfg = chart.options.plugins.alarmLines
    if (!cfg || !cfg.probes) return
    const { ctx, chartArea, scales } = chart
    const y = scales.y
    ctx.save()
    ctx.font = "10px sans-serif"
    ctx.textBaseline = "middle"
    cfg.probes.forEach((p) => {
      ;[
        ["max", p.alarm_max, p.color.line, "▲"],
        ["min", p.alarm_min, p.color.faint, "▼"],
      ].forEach(([kind, val, stroke, glyph]) => {
        if (val == null) return
        const py = y.getPixelForValue(val)
        if (py < chartArea.top || py > chartArea.bottom) return
        ctx.beginPath()
        ctx.setLineDash(kind === "max" ? [6, 4] : [2, 4])
        ctx.lineWidth = 1
        ctx.strokeStyle = stroke
        ctx.moveTo(chartArea.left, py)
        ctx.lineTo(chartArea.right, py)
        ctx.stroke()
        ctx.setLineDash([])
        const label = `${p.short}${glyph}${Math.round(val)}`
        ctx.fillStyle = stroke
        ctx.textAlign = "right"
        ctx.fillText(label, chartArea.right - 4, py - 6)
      })
    })
    ctx.restore()
  },
}

export default {
  name: "LineChart",
  // eslint-disable-next-line
  components: { Line },
  data: () => ({
    loaded: false,
    error: false,
    stopped: false,
    data: null,
    chartData: null,
    alarmLinesPlugin,
  }),
  computed: {
    probes() {
      if (!this.data) return []
      return probeKeys(this.data).map((k, i) => ({
        key: k,
        short: `P${k.split("_")[1]}`,
        label: `Probe ${k.split("_")[1]}`,
        color: PROBE_COLORS[i % PROBE_COLORS.length],
        current_temp: this.data[k].current_temp,
        alarm_min: this.data[k].alarm_min,
        alarm_max: this.data[k].alarm_max,
        state: this.stateOf(this.data[k]),
      }))
    },
    billows() {
      return !!(this.data && this.data.billows)
    },
    // Text alternative for the canvas chart (which screen readers can't read).
    chartSummary() {
      if (!this.probes.length) return "Temperature history chart"
      const parts = this.probes.map((p) => {
        const state = p.state === "ok" ? "" : ` (${this.stateLabel(p.state)})`
        return `${p.label} ${this.fmt(p.current_temp)}°F${state}`
      })
      return `Temperature history over about 4 hours. Latest readings: ${parts.join(
        ", "
      )}.`
    },
    options() {
      const alarmVals = this.probes.flatMap((p) => [p.alarm_min, p.alarm_max])
      const finite = alarmVals.filter((v) => typeof v === "number")
      const c = isDark.value ? CHART_COLORS.dark : CHART_COLORS.light
      return {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: {
          legend: {
            position: "bottom",
            labels: { color: c.legend, usePointStyle: true },
          },
          tooltip: {
            callbacks: {
              title: (items) => {
                const x = items[0].parsed.x
                return x === 0 ? "now" : `${-x} min ago`
              },
            },
          },
          // Consumed by alarmLinesPlugin above.
          alarmLines: { probes: this.probes },
        },
        scales: {
          x: {
            type: "linear",
            title: {
              display: true,
              text: "Minutes before now",
              color: c.tick,
            },
            border: { display: true, color: c.grid },
            grid: { display: true, drawOnChartArea: true, color: c.grid },
            ticks: {
              color: c.tick,
              callback: (v) => (v === 0 ? "now" : `${v}m`),
            },
          },
          y: {
            border: { display: true, color: c.grid },
            grid: { color: c.grid },
            ticks: { color: c.tick },
            suggestedMin: finite.length ? Math.min(...finite) - 10 : undefined,
            suggestedMax: finite.length ? Math.max(...finite) + 10 : undefined,
          },
        },
      }
    },
  },
  created: function () {
    this.poll()
  },
  beforeUnmount: function () {
    this.stopped = true
    clearTimeout(this.timer)
  },
  methods: {
    fmt(v) {
      return typeof v === "number" ? Math.round(v * 10) / 10 : "--"
    },
    stateOf(probe) {
      const t = probe.current_temp
      if (typeof t !== "number") return "ok"
      if (typeof probe.alarm_max === "number" && t >= probe.alarm_max)
        return "high"
      if (typeof probe.alarm_min === "number" && t <= probe.alarm_min)
        return "low"
      return "ok"
    },
    stateLabel(state) {
      return state === "high" ? "HIGH" : state === "low" ? "LOW" : "OK"
    },
    convertData(data) {
      const keys = probeKeys(data)
      if (!keys.length) return null
      return {
        datasets: keys.map((k, i) => {
          const history = data[k].history
          const last = history.length - 1
          const color = PROBE_COLORS[i % PROBE_COLORS.length]
          return {
            label: `Probe ${k.split("_")[1]}`,
            // x is minutes before now: oldest sample is most negative, the
            // newest is 0. This stays honest across polls because it never
            // implies an absolute wall-clock time.
            data: history.map((y, idx) => ({
              x: ((idx - last) * SAMPLE_SECONDS) / 60,
              y,
            })),
            fill: false,
            borderColor: color.line,
            // Distinct texture + legend marker per probe (colorblind-safe).
            borderDash: color.dash,
            pointStyle: color.point,
            tension: 0,
            pointRadius: 2,
          }
        }),
      }
    },
    async poll() {
      await this.getData()
      if (!this.stopped) this.timer = setTimeout(this.poll, 30000)
    },
    async getData() {
      try {
        const data = await getJSON("data")
        this.data = data
        this.chartData = this.convertData(data)
        this.error = false
      } catch (error) {
        this.error = true
      } finally {
        this.loaded = true
      }
    },
  },
}
</script>

<style scoped>
.banner {
  max-width: 720px;
  margin: 1em auto;
  padding: 0.75em 1em;
  border-radius: 0.5em;
  text-align: center;
  font-family: Avenir, Helvetica, Arial, sans-serif;
  font-weight: bold;
}
.banner-error {
  background-color: var(--alarm-high-bg);
  color: var(--alarm-high-fg);
}

.probe-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 1em;
  max-width: 720px;
  margin: 1em auto;
  padding: 0 1em;
  box-sizing: border-box;
}

.probe-card {
  border: 1px solid var(--border);
  border-left: 4px solid var(--probe-color);
  border-radius: 0.5em;
  padding: 0.75em 1em;
  background: var(--surface);
  box-shadow: 0 0 1em var(--shadow);
  font-family: Avenir, Helvetica, Arial, sans-serif;
  color: var(--text);
}
.probe-card.state-high {
  background-color: var(--alarm-high-bg);
}
.probe-card.state-low {
  background-color: var(--alarm-low-bg);
}

.probe-card-head {
  display: flex;
  align-items: center;
  gap: 0.4em;
  font-size: 0.85em;
}
.probe-dot {
  width: 0.7em;
  height: 0.7em;
  border-radius: 50%;
  background-color: var(--probe-color);
}
.probe-label {
  font-weight: bold;
}
.probe-badge {
  margin-left: auto;
  font-size: 0.7em;
  font-weight: bold;
  padding: 0.1em 0.5em;
  border-radius: 1em;
  background-color: var(--border);
  color: var(--text-muted);
}
.state-high .probe-badge {
  background-color: var(--alarm-high-fg);
  color: #fff;
}
.state-low .probe-badge {
  background-color: var(--alarm-low-fg);
  color: #fff;
}

.probe-temp {
  font-size: 2em;
  font-weight: bold;
  line-height: 1.1;
  margin: 0.15em 0;
}
.probe-unit {
  font-size: 0.5em;
  font-weight: normal;
  margin-left: 0.15em;
  color: var(--text-muted);
}
.probe-range {
  font-size: 0.8em;
  color: var(--text-muted);
}

.billows-chip {
  max-width: 720px;
  margin: 0 auto 0.5em;
  padding: 0 1em;
  font-family: Avenir, Helvetica, Arial, sans-serif;
  font-size: 0.85em;
  font-weight: bold;
  color: var(--accent);
  text-align: center;
}
</style>

import { Line, mixins } from "vue-chartjs"
const { reactiveData } = mixins

export default {
  extends: Line,
  mixins: [reactiveData],
  props: {
    chartData: {
      type: Object,
      default: null,
    },
    options: {
      type: Object,
      default: null,
    },
  },
  mounted() {
    this.renderChart(this.chartData, this.options)
  },
}

module.exports = {
  root: true,
  env: {
    node: true,
  },
  extends: [
    "eslint:recommended",
    "plugin:vue/recommended",
    "plugin:vue/essential",
  ],
  parser: "vue-eslint-parser",
  rules: {
    "semi": ["warn", "never"],
    "no-unreachable": ["warn"],
  },
}

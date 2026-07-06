// Shared handler for FormKit password inputs: toggles the reveal (eye) icon
// and the input type when the suffix icon is clicked. Wire it up with:
//   suffix-icon="eyeClosed" @suffix-icon-click="togglePasswordVisibility"
export function togglePasswordVisibility(node) {
  node.props.suffixIcon = node.props.suffixIcon === "eye" ? "eyeClosed" : "eye"
  node.props.type = node.props.type === "password" ? "text" : "password"
}

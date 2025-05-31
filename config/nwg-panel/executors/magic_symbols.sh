#!/bin/bash

symbols="꙰҉҂⛧𖤐☠⛓†༒⚝𐂃𐰴𖣘꧁꧂ᓭ⚚♆∷ʬ⟁⛤₪₷₥⌬☢⛩ᛉᚲ𖤓⩿𝌆᙮✠𓆩𓆪𓂀𓄿𓇋ꓷ𒀭�ͲͳͶͷͺͻ̷̸̶͏̀́͜͢͡͠𝖑𝖘𝖃𝕯𝕸𝖅␉ɃŁƧЖѮ⌇⧖"
symbols_array=()

# Split each character into an array
while IFS= read -r -n1 char; do
    symbols_array+=("$char")
done <<< "$symbols"

length=$((RANDOM % 5 + 18))

# Build random string
result=""
for ((i=0; i<length; i++)); do
    rand_index=$((RANDOM % ${#symbols_array[@]}))
    result+="${symbols_array[$rand_index]} "
done

echo "$result"


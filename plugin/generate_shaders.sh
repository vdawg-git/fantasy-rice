OUTFILE="shaders.hpp"

rm -f "$OUTFILE"
echo '// Auto-generated shader header' > "$OUTFILE"

for f in *.frag; do
    # Convert "visualizer_frag.glsl" → "visualizer_frag_glsl"
    varname=$(echo "$f" | sed 's/\./_/g')
    echo "const char* $varname = R\"glsl(" >> "$OUTFILE"
    cat "$f" >> "$OUTFILE"
    echo ")glsl\";" >> "$OUTFILE"
done

// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

// Auto generated defines for icon-font with `fontcustom`

namespace Font
{
  enum Icon {
<%  @glyphs.each do |key, value|
      name = key.to_s.delete_prefix("iconmonstr-")
      name = name.gsub(/^[0-9]|[^A-Za-z0-9]/, '_')
%><%= "    #{name} = 0x#{value[:codepoint].to_s(16)}, // #{value[:source]}" %>
<% end
%>  };
}

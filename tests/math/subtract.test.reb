; functions/math/subtract.r
[1 == subtract 3 2]
; integer -9223372036854775808 - x tests
#64bit
[0 == subtract -9223372036854775808 -9223372036854775808]
#64bit
[-1 == subtract -9223372036854775808 -9223372036854775807]
#64bit
[-9223372036854775807 == subtract -9223372036854775808 -1]
#64bit
[-9223372036854775808 = subtract -9223372036854775808 0]
#64bit
[error? try [subtract -9223372036854775808 1]]
#64bit
[error? try [subtract -9223372036854775808 9223372036854775806]]
#64bit
[error? try [subtract -9223372036854775808 9223372036854775807]]
; integer -9223372036854775807 - x tests
#64bit
[1 = subtract -9223372036854775807 -9223372036854775808]
#64bit
[0 = subtract -9223372036854775807 -9223372036854775807]
#64bit
[-9223372036854775806 = subtract -9223372036854775807 -1]
#64bit
[-9223372036854775807 = subtract -9223372036854775807 0]
#64bit
[-9223372036854775808 = subtract -9223372036854775807 1]
#64bit
[error? try [subtract -9223372036854775807 9223372036854775806]]
#64bit
[error? try [subtract -9223372036854775807 9223372036854775807]]
; integer -2147483648 - x tests
[0 = subtract -2147483648 -2147483648]
[-2147483647 = subtract -2147483648 -1]
[-2147483648 = subtract -2147483648 0]
#32bit
[error? try [subtract -2147483648 1]]
#64bit
[-2147483649 = subtract -2147483648 1]
#32bit
[error? try [subtract -2147483648 2147483647]]
#64bit
[-4294967295 = subtract -2147483648 2147483647]
; integer -1 - x tests
#64bit
[9223372036854775807 = subtract -1 -9223372036854775808]
#64bit
[9223372036854775806 = subtract -1 -9223372036854775807]
[0 = subtract -1 -1]
[-1 = subtract -1 0]
[-2 = subtract -1 1]
#64bit
[-9223372036854775807 = subtract -1 9223372036854775806]
#64bit
[-9223372036854775808 = subtract -1 9223372036854775807]
; integer 0 - x tests
#64bit
[error? try [subtract 0 -9223372036854775808]]
#32bit
[error? try [subtract 0 -2147483648]]
#64bit
[2147483648 = subtract 0 -2147483648]
#64bit
[9223372036854775807 = subtract 0 -9223372036854775807]
[1 = subtract 0 -1]
[0 = subtract 0 0]
[-1 = subtract 0 1]
#64bit
[-9223372036854775806 = subtract 0 9223372036854775806]
#64bit
[-9223372036854775807 = subtract 0 9223372036854775807]
; integer 1 - x tests
#64bit
[error? try [subtract 1 -9223372036854775808]]
#64bit
[error? try [subtract 1 -9223372036854775807]]
[2 = subtract 1 -1]
[1 = subtract 1 0]
[0 = subtract 1 1]
#64bit
[-9223372036854775805 = subtract 1 9223372036854775806]
#64bit
[-9223372036854775806 = subtract 1 9223372036854775807]
; integer 2147483647 + x
#32bit
[error? try [subtract 2147483647 -2147483648]]
#64bit
[4294967295 = subtract 2147483647 -2147483648]
#32bit
[error? try [subtract 2147483647 -1]]
#64bit
[2147483648 = subtract 2147483647 -1]
[2147483647 = subtract 2147483647 0]
#32bit
[2147483646 = subtract 2147483647 1]
#32bit
[0 = subtract 2147483647 2147483647]
; integer 9223372036854775806 - x tests
#64bit
[error? try [subtract 9223372036854775806 -9223372036854775808]]
#64bit
[error? try [subtract 9223372036854775806 -9223372036854775807]]
#64bit
[9223372036854775807 = subtract 9223372036854775806 -1]
#64bit
[9223372036854775806 = subtract 9223372036854775806 0]
#64bit
[9223372036854775805 = subtract 9223372036854775806 1]
#64bit
[0 = subtract 9223372036854775806 9223372036854775806]
#64bit
[-1 = subtract 9223372036854775806 9223372036854775807]
; integer 9223372036854775807 - x tests
#64bit
[error? try [subtract 9223372036854775807 -9223372036854775808]]
#64bit
[error? try [subtract  9223372036854775807 -9223372036854775807]]
#64bit
[error? try [subtract 9223372036854775807 -1]]
#64bit
[9223372036854775807 = subtract 9223372036854775807 0]
#64bit
[9223372036854775806 = subtract 9223372036854775807 1]
#64bit
[1 = subtract 9223372036854775807 9223372036854775806]
#64bit
[0 = subtract 9223372036854775807 9223372036854775807]
; decimal - integer
[0.1 = subtract 1.1 1]
[-2147483648.0 = subtract -1.0 2147483647]
[2147483649.0 = subtract 1.0 -2147483648]
; integer - decimal
[-0.1 = subtract 1 1.1]
[2147483648.0 = subtract 2147483647 -1.0]
[-2147483649.0 = subtract -2147483648 1.0]
; -1.7976931348623157e308 - decimal
[0.0 = subtract -1.7976931348623157e308 -1.7976931348623157e308]
[-1.7976931348623157e308 = subtract -1.7976931348623157e308 -1.0]
[-1.7976931348623157e308 = subtract -1.7976931348623157e308 -4.94065645841247E-324]
[-1.7976931348623157e308 = subtract -1.7976931348623157e308 0.0]
[-1.7976931348623157e308 = subtract -1.7976931348623157e308 4.94065645841247E-324]
[-1.7976931348623157e308 = subtract -1.7976931348623157e308 1.0]
[error? try [subtract -1.7976931348623157e308 1.7976931348623157e308]]
; -1.0 + decimal
[1.7976931348623157e308 = subtract -1.0 -1.7976931348623157e308]
[0.0 = subtract -1.0 -1.0]
[-1.0 = subtract -1.0 -4.94065645841247E-324]
[-1.0 = subtract -1.0 0.0]
[-1.0 = subtract -1.0 4.94065645841247E-324]
[-2.0 = subtract -1.0 1.0]
[-1.7976931348623157e308 = subtract -1.0 1.7976931348623157e308]
; -4.94065645841247E-324 + decimal
[1.7976931348623157e308 = subtract -4.94065645841247E-324 -1.7976931348623157e308]
[1.0 = subtract -4.94065645841247E-324 -1.0]
[0.0 = subtract -4.94065645841247E-324 -4.94065645841247E-324]
[-4.94065645841247E-324 = subtract -4.94065645841247E-324 0.0]
[-9.88131291682493E-324 = subtract -4.94065645841247E-324 4.94065645841247E-324]
[-1.0 = subtract -4.94065645841247E-324 1.0]
[-1.7976931348623157e308 = subtract -4.94065645841247E-324 1.7976931348623157e308]
; 0.0 + decimal
[1.7976931348623157e308 = subtract 0.0 -1.7976931348623157e308]
[1.0 = subtract 0.0 -1.0]
[4.94065645841247E-324 = subtract 0.0 -4.94065645841247E-324]
[0.0 = subtract 0.0 0.0]
[-4.94065645841247E-324 = subtract 0.0 4.94065645841247E-324]
[-1.0 = subtract 0.0 1.0]
[-1.7976931348623157e308 = subtract 0.0 1.7976931348623157e308]
; 4.94065645841247E-324 + decimal
[1.7976931348623157e308 = subtract 4.94065645841247E-324 -1.7976931348623157e308]
[1.0 = subtract 4.94065645841247E-324 -1.0]
[9.88131291682493E-324 = subtract 4.94065645841247E-324 -4.94065645841247E-324]
[4.94065645841247E-324 = subtract 4.94065645841247E-324 0.0]
[0.0 = subtract 4.94065645841247E-324 4.94065645841247E-324]
[-1.0 = subtract 4.94065645841247E-324 1.0]
[-1.7976931348623157e308 = subtract 4.94065645841247E-324 1.7976931348623157e308]
; 1.0 + decimal
[1.7976931348623157e308 = subtract 1.0 -1.7976931348623157e308]
[2.0 = subtract 1.0 -1.0]
[1.0 = subtract 1.0 4.94065645841247E-324]
[1.0 = subtract 1.0 0.0]
[1.0 = subtract 1.0 -4.94065645841247E-324]
[0.0 = subtract 1.0 1.0]
[-1.7976931348623157e308 = subtract 1.0 1.7976931348623157e308]
; 1.7976931348623157e308 + decimal
[error? try [subtract 1.7976931348623157e308 -1.7976931348623157e308]]
[1.7976931348623157e308 = subtract 1.7976931348623157e308 -1.0]
[1.7976931348623157e308 = subtract 1.7976931348623157e308 -4.94065645841247E-324]
[1.7976931348623157e308 = subtract 1.7976931348623157e308 0.0]
[1.7976931348623157e308 = subtract 1.7976931348623157e308 4.94065645841247E-324]
[1.7976931348623157e308 = subtract 1.7976931348623157e308 1.0]
[0.0 = subtract 1.7976931348623157e308 1.7976931348623157e308]
; pair
[0x0 = subtract -2147483648x-2147483648 -2147483648x-2147483648]
[-2147483647x-2147483647 = subtract -2147483648x-2147483648 -1x-1]
[-2147483648x-2147483648 = subtract -2147483648x-2147483648 0x0]
[0x0 = subtract -1x-1 -1x-1]
[-1x-1 = subtract -1x-1 0x0]
[-2x-2 = subtract -1x-1 1x1]
[2147483648x2147483648 = subtract 0x0 -2147483648x-2147483648]
[1x1 = subtract 0x0 -1x-1]
[0x0 = subtract 0x0 0x0]
[-1x-1 = subtract 0x0 1x1]
[-2147483647x-2147483647 = subtract 0x0 2147483647x2147483647]
[2x2 = subtract 1x1 -1x-1]
[1x1 = subtract 1x1 0x0]
[0x0 = subtract 1x1 1x1]
[2147483647x2147483647 = subtract 2147483647x2147483647 0x0]
[0x0 = subtract 2147483647x2147483647 2147483647x2147483647]
; char
[0.0.0 = subtract 0.0.0 0.0.0]
[0.0.0 = subtract 0.0.0 0.0.1]
[0.0.0 = subtract 0.0.0 0.0.255]
[0.0.1 = subtract 0.0.1 0.0.0]
[0.0.0 = subtract 0.0.1 0.0.1]
[0.0.0 = subtract 0.0.1 0.0.255]
[0.0.255 = subtract 0.0.255 0.0.0]
[0.0.254 = subtract 0.0.255 0.0.1]
[0.0.0 = subtract 0.0.255 0.0.255]
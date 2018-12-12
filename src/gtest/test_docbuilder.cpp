// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>
#include "rpc/docbuilder.h"
#include "rpc/server.h"

TEST(DocBuilderTests, PrimitiveArgumentCliHeaderString) {
    EXPECT_EQ("boolArg", RpcArgument::Boolean("boolArg", "").CliHeaderString());
    EXPECT_EQ("(boolArg)", RpcArgument::OptionalBoolean("boolArg", "", "").CliHeaderString());
    EXPECT_EQ("intArg", RpcArgument::Integer("intArg", "").CliHeaderString());
    EXPECT_EQ("(intArg)", RpcArgument::OptionalInteger("intArg", "", "").CliHeaderString());
    EXPECT_EQ("decArg", RpcArgument::Decimal("decArg", "").CliHeaderString());
    EXPECT_EQ("(decArg)", RpcArgument::OptionalDecimal("decArg", "", "").CliHeaderString());
    EXPECT_EQ("\"strArg\"", RpcArgument::String("strArg", "").CliHeaderString());
    EXPECT_EQ("(\"strArg\")", RpcArgument::OptionalString("strArg", "", "").CliHeaderString());
    EXPECT_EQ("\"hexStrArg\"", RpcArgument::HexString("hexStrArg", "").CliHeaderString());
    EXPECT_EQ("(\"hexStrArg\")", RpcArgument::OptionalHexString("hexStrArg", "", "").CliHeaderString());
}

TEST(DocBuilderTests, ThreeArgumentsOneResult) {
    std::string testDoc = RpcDocBuilder("testrpc")
        .SetDescription("This is a description.")
        .AddArgument(RpcArgument::Boolean("arg0", "This is a description of an argument."))
        .AddArgument(RpcArgument::OptionalString("arg1", "value1", "This is an optional argument."))
        .AddArgument(RpcArgument::OptionalString("arg02","value2", "This is an optional argument."))
        .AddResult(RpcArgument::String("This is a result."))
        .ToString();
    std::string expectedDoc = "testrpc arg0 (\"arg1\") (\"arg02\")\n"
        "\n"
        "This is a description.\n"
        "\n"
        "Arguments:\n"
        "1. arg0     (boolean, required) This is a description of an argument.\n"
        "2. \"arg1\"   (string, optional, default=\"value1\") This is an optional argument.\n"
        "3. \"arg02\"  (string, optional, default=\"value2\") This is an optional argument.\n"
        "\n"
        "Result:\n"
        "\"xxx\"  (string) This is a result.\n";
    EXPECT_EQ(expectedDoc, testDoc);
}

TEST(DocBuilderTests, NoArgumentsOneResult) {
    std::string testDoc = RpcDocBuilder("testrpc")
        .SetDescription("This is a description.")
        .AddResult(RpcArgument::String("This is a result."))
        .ToString();
    std::string expectedDoc = "testrpc\n"
        "\n"
        "This is a description.\n"
        "\n"
        "Result:\n"
        "\"xxx\"  (string) This is a result.\n";
    EXPECT_EQ(expectedDoc, testDoc);
}

TEST(DocBuilderTests, MultipleResults) {
    std::string testDoc = RpcDocBuilder("testrpc")
        .SetDescription("This is a description.")
        .AddArgument(RpcArgument::String("stringArg", "This is a string argument."))
        .AddResult("result #1",
            RpcArgument::String("This is a string result."))
        .AddResult("result #2",
            RpcArgument::Array("This is an array result.")
                .Of(RpcArgument::String("This is a string result in an array.")))
        .AddResult("result #3",
            RpcArgument::Object("This is an object result.")
                .With(RpcArgument::String("stringRes1", "This is a string result in an object."))
                .With(RpcArgument::String("stringRes2", "This is another string result in an object.")))
        .ToString();
    std::string expectedDoc = "testrpc \"stringArg\"\n"
        "\n"
        "This is a description.\n"
        "\n"
        "Arguments:\n"
        "1. \"stringArg\"  (string, required) This is a string argument.\n"
        "\n"
        "Result (result #1):\n"
        "\"xxx\"  (string) This is a string result.\n"
        "\n"
        "Result (result #2):\n"
        "[         (json array of strings) This is an array result.\n"
        "  \"xxx\",  (string) This is a string result in an array.\n"
        "  ...\n"
        "]\n"
        "\n"
        "Result (result #3):\n"
        "{                       (json object) This is an object result.\n"
        "  \"stringRes1\": \"xxx\",  (string) This is a string result in an object.\n"
        "  \"stringRes2\": \"xxx\"   (string) This is another string result in an object.\n"
        "}\n";
    EXPECT_EQ(expectedDoc, testDoc);
}

TEST(DocBuilderTests, HelpExamples) {
    std::string testDoc = RpcDocBuilder("testrpc")
        .SetDescription("This is a description.")
        .AddArgument(RpcArgument::Boolean("arg0", "This is a description of an argument."))
        .AddArgument(RpcArgument::OptionalString("arg1", "value1", "This is an optional argument."))
        .AddResult(RpcArgument::String("This is a result."))
        .AddHelpExampleCli("true")
        .AddHelpExampleRpc("This is a description of an example", "true \"string\"")
        .ToString();
    std::string expectedDoc = "testrpc arg0 (\"arg1\")\n"
        "\n"
        "This is a description.\n"
        "\n"
        "Arguments:\n"
        "1. arg0    (boolean, required) This is a description of an argument.\n"
        "2. \"arg1\"  (string, optional, default=\"value1\") This is an optional argument.\n"
        "\n"
        "Result:\n"
        "\"xxx\"  (string) This is a result.\n"
        "\n"
        "Examples:\n"
        + HelpExampleCli("testrpc", "true")
        + "\n"
        "This is a description of an example\n"
        + HelpExampleRpc("testrpc", "true \"string\"");
    EXPECT_EQ(expectedDoc, testDoc);
}

TEST(DocBuilderTests, ArrayOfStringsArgument) {
    std::string testDoc = RpcDocBuilder("testrpc")
        .SetDescription("This is a description.")
        .AddArgument(RpcArgument::Array("arrayArg", "This is an array argument.")
            .Of(RpcArgument::String("stringArg", "This is a string argument.")))
        .AddResult(RpcArgument::Array("This is an array result.")
            .Of(RpcArgument::String("This is a string result."))) 
        .ToString();
    std::string expectedDoc = "testrpc [\"stringArg\", ...]\n"
        "\n"
        "This is a description.\n"
        "\n"
        "Arguments:\n"
        "1. arrayArg          (json array of strings, required) This is an array argument.\n"
        "     [\n"
        "       \"stringArg\",  (string, required) This is a string argument.\n"
        "       ...\n"
        "     ]\n"
        "\n"
        "Result:\n"
        "[         (json array of strings) This is an array result.\n"
        "  \"xxx\",  (string) This is a string result.\n"
        "  ...\n"
        "]\n";
    EXPECT_EQ(expectedDoc, testDoc);
}

TEST(DocBuilderTests, NestedArrayArguments) {
    ArrayArgument arrayArgument = RpcArgument::Array("outerArray", "This is an outer array argument.")
        .Of(RpcArgument::Array()
            .Of(RpcArgument::Array()
                .Of(RpcArgument::String("stringArg", "This is a string argument."))));
    std::string nestedArgumentString = "1. " + arrayArgument.ArgumentString(0, arrayArgument.ArgumentNameLength(true), true);
    std::string expectedArgumentString = 
        "1. outerArray            (json array of json arrays of json arrays of strings, required) This is an outer array argument.\n"
        "     [\n"
        "       [                 (json array of json arrays of strings, required)\n"
        "         [               (json array of strings, required)\n"
        "           \"stringArg\",  (string, required) This is a string argument.\n"
        "           ...\n"
        "         ],\n"
        "         ...\n"
        "       ],\n"
        "       ...\n"
        "     ]";
    EXPECT_EQ(expectedArgumentString, nestedArgumentString);
}

TEST(DocBuilderTests, NestedArrayResults) {
    ArrayArgument arrayArgument = RpcArgument::Array("This is an outer array result.")
        .Of(RpcArgument::Array("This is a middle array result.")
            .Of(RpcArgument::Array("This is an inner array result.")
                .Of(RpcArgument::String("This is a string result."))));
    std::string nestedResultString = arrayArgument.ArgumentString(0, arrayArgument.ArgumentNameLength(false), false);
    std::string expectedResultString =
        "[             (json array of json arrays of json arrays of strings) This is an outer array result.\n"
        "  [           (json array of json arrays of strings) This is a middle array result.\n"
        "    [         (json array of strings) This is an inner array result.\n"
        "      \"xxx\",  (string) This is a string result.\n"
        "      ...\n"
        "    ],\n"
        "    ...\n"
        "  ],\n"
        "  ...\n"
        "]";
    EXPECT_EQ(expectedResultString, nestedResultString);
}

TEST(DocBuilderTests, ArrayOfObjectArgumentInHeader) {
    std::string argumentHeaderString = RpcArgument::Array()
        .Of(RpcArgument::Object("This is an object argument")
            .With(RpcArgument::String("stringArg", "This is a string argument"))
            .With(RpcArgument::Decimal("decimalArg", "This is a decimal argument."))
            .With(RpcArgument::OptionalBoolean("boolArg", "true", "This is an optional bool.")))
        .CliHeaderString();
    std::string expectedHeaderString = "[{\"stringArg\": \"xxx\", \"decimalArg\": x.xxx, (\"boolArg\": true|false)}, ...]";
    EXPECT_EQ(expectedHeaderString, argumentHeaderString);
}

TEST(DocBuilderTests, NestedObjectArguments) {
    ObjectArgument objectArgument = RpcArgument::Object("outerObject", "This is an outer object argument.")
        .With(RpcArgument::Array("innerArray", "")
            .Of(RpcArgument::Object()
                .With(RpcArgument::String("stringArg", "This is a string argument."))
                .With(RpcArgument::Integer("intArg", "This is an integer argument."))))
        .With(RpcArgument::OptionalBoolean("boolArg", "true", "This is a boolean argument."));
    std::string nestedArgumentString = "1. " + objectArgument.ArgumentString(0, objectArgument.ArgumentNameLength(true), true);
    std::string expectedArgumentString = 
        "1. outerObject                  (json object, required) This is an outer object argument.\n"
        "     {\n"
        "       \"innerArray\": [          (json array of json objects, required)\n"
        "         {                      (json object, required)\n"
        "           \"stringArg\": \"xxx\",  (string, required) This is a string argument.\n"
        "           \"intArg\": n          (numeric, required) This is an integer argument.\n"
        "         },\n"
        "         ...\n"
        "       ],\n"
        "       \"boolArg\": true|false    (boolean, optional, default=true) This is a boolean argument.\n"
        "     }";
    EXPECT_EQ(expectedArgumentString, nestedArgumentString);
}

TEST(DocBuilderTests, NestedObjectResult) {
    ObjectArgument objectResult = RpcArgument::Object("This is an outer object result.")
        .With(RpcArgument::Array("innerArray", "This is an inner array result.")
            .Of(RpcArgument::Object("This is an inner object result.")
                .With(RpcArgument::String("stringRes", "This is a string result."))
                .With(RpcArgument::Integer("intRes", "This is an integer result."))))
        .With(RpcArgument::OptionalBoolean("boolRes", "", "This is a boolean result."));
    std::string nestedResultString = objectResult.ArgumentString(0, objectResult.ArgumentNameLength(false), false);
    std::string expectedResultString = 
        "{                          (json object) This is an outer object result.\n"
        "  \"innerArray\": [          (json array of json objects) This is an inner array result.\n"
        "    {                      (json object) This is an inner object result.\n"
        "      \"stringRes\": \"xxx\",  (string) This is a string result.\n"
        "      \"intRes\": n          (numeric) This is an integer result.\n"
        "    },\n"
        "    ...\n"
        "  ],\n"
        "  \"boolRes\": true|false    (boolean, optional) This is a boolean result.\n"
        "}";
    EXPECT_EQ(expectedResultString, nestedResultString);
}

TEST(DocBuilderTests, AdditionalDescriptionArrayArgument) {
    ArrayArgument arrayArgument = RpcArgument::Array("outerArray", "This is an outer array argument.")
        .AddDescription("It has a long description.")
        .AddDescription("Its description spans multiple lines.")
        .Of(RpcArgument::String("stringArg", "This is a string argument.")
            .AddDescription("It has a long description too.")
            .AddDescription("Its description also spans multiple multiple lines."));
    std::string argumentString = "1. " + arrayArgument.ArgumentString(0, arrayArgument.ArgumentNameLength(true), true);
    std::string expectedArgumentString =
        "1. outerArray        (json array of strings, required) This is an outer array argument.\n"
        "                     It has a long description.\n"
        "                     Its description spans multiple lines.\n"
        "     [\n"
        "       \"stringArg\",  (string, required) This is a string argument.\n"
        "                     It has a long description too.\n"
        "                     Its description also spans multiple multiple lines.\n"
        "       ...\n"
        "     ]";
    EXPECT_EQ(expectedArgumentString, argumentString);
}

TEST(DocBuilderTests, AdditionalDescriptionArrayResult) {
    ArrayArgument arrayResult = RpcArgument::Array("This is an outer array result.")
        .AddDescription("It has a long description.")
        .AddDescription("Its description spans multiple lines.")
        .Of(RpcArgument::String("This is a string result.")
            .AddDescription("It has a long description too.")
            .AddDescription("Its description also spans multiple multiple lines."));
    std::string resultString = arrayResult.ArgumentString(0, arrayResult.ArgumentNameLength(false), false);
    std::string expectedResultString =
        "[         (json array of strings) This is an outer array result.\n"
        "          It has a long description.\n"
        "          Its description spans multiple lines.\n"
        "  \"xxx\",  (string) This is a string result.\n"
        "          It has a long description too.\n"
        "          Its description also spans multiple multiple lines.\n"
        "  ...\n"
        "]";
    EXPECT_EQ(expectedResultString, resultString);
}

TEST(DocBuilderTests, AdditionalDescriptionObjectArgument) {
    ObjectArgument objectArgument = RpcArgument::Object("outerObject", "This is an outer object argument.")
        .AddDescription("It has a long description.")
        .AddDescription("Its description spans multiple lines.")
        .With(RpcArgument::String("stringArg", "This is a string argument.")
            .AddDescription("It has a long description too.")
            .AddDescription("Its description also spans multiple multiple lines."))
        .With(RpcArgument::Integer("intArg", "This is an integer argument.")
            .AddDescription("The int argument also has a long description."));
    std::string argumentString = "1. " + objectArgument.ArgumentString(0, objectArgument.ArgumentNameLength(true), true);
    std::string expectedArgumentString =
        "1. outerObject              (json object, required) This is an outer object argument.\n"
        "                            It has a long description.\n"
        "                            Its description spans multiple lines.\n"
        "     {\n"
        "       \"stringArg\": \"xxx\",  (string, required) This is a string argument.\n"
        "                            It has a long description too.\n"
        "                            Its description also spans multiple multiple lines.\n"
        "       \"intArg\": n          (numeric, required) This is an integer argument.\n"
        "                            The int argument also has a long description.\n"
        "     }";
    EXPECT_EQ(expectedArgumentString, argumentString);
}

TEST(DocBuilderTests, AdditionalDescriptionObjectResult) {
    ObjectArgument objectResult = RpcArgument::Object("This is an outer object result.")
        .AddDescription("It has a long description.")
        .AddDescription("Its description spans multiple lines.")
        .With(RpcArgument::String("stringRes", "This is a string result.")
            .AddDescription("It has a long description too.")
            .AddDescription("Its description also spans multiple multiple lines."))
        .With(RpcArgument::Integer("intRes", "This is an integer result.")
            .AddDescription("The int result also has a long description."));
    std::string resultString = objectResult.ArgumentString(0, objectResult.ArgumentNameLength(false), false);
    std::string expectedResultString =
        "{                      (json object) This is an outer object result.\n"
        "                       It has a long description.\n"
        "                       Its description spans multiple lines.\n"
        "  \"stringRes\": \"xxx\",  (string) This is a string result.\n"
        "                       It has a long description too.\n"
        "                       Its description also spans multiple multiple lines.\n"
        "  \"intRes\": n          (numeric) This is an integer result.\n"
        "                       The int result also has a long description.\n"
        "}";
    EXPECT_EQ(expectedResultString, resultString);
}

TEST(DocBuilderTests, OptionalObjectWithOptionalArray) {
    ObjectArgument objectArgument = RpcArgument::OptionalObject("outerObject", "This is an optional outer object argument.")
        .With(RpcArgument::String("stringArg", "This is a string argument."))
        .With(RpcArgument::Decimal("decimalArg", "This is a decimal argument."))
        .With(RpcArgument::OptionalArray("arrayArg", "This is an optional array.")
            .Of(RpcArgument::HexString("hexArg", "This is a hex string argument.")));
    std::string headerString = objectArgument.CliHeaderString();
    std::string expectedHeaderString = "({\"stringArg\": \"xxx\", \"decimalArg\": x.xxx, (\"arrayArg\": [\"hexArg\", ...])})";
    EXPECT_EQ(expectedHeaderString, headerString);
    std::string argumentString = "1. " + objectArgument.ArgumentString(0, objectArgument.ArgumentNameLength(true), true);
    std::string expectedArgumentString =
        "1. outerObject               (json object, optional) This is an optional outer object argument.\n"
        "     {\n"
        "       \"stringArg\": \"xxx\",   (string, required) This is a string argument.\n"
        "       \"decimalArg\": x.xxx,  (numeric, required) This is a decimal argument.\n"
        "       \"arrayArg\": [         (json array of strings, optional) This is an optional array.\n"
        "         \"hexArg\",           (string, required) This is a hex string argument.\n"
        "         ...\n"
        "       ]\n"
        "     }";
    EXPECT_EQ(expectedArgumentString, argumentString);
}

TEST(DocBuilderTests, BlankDefaltsAreHidden) {
    PrimitiveArgument hexArg = RpcArgument::OptionalHexString("hexArg", "", "This is a hex string argument.");
    EXPECT_EQ("\"hexArg\"  (string, optional) This is a hex string argument.", hexArg.ArgumentString(0, hexArg.ArgumentNameLength(true), true));
    PrimitiveArgument intArg = RpcArgument::OptionalInteger("intArg", "", "This is an integer argument.");
    EXPECT_EQ("intArg  (numeric, optional) This is an integer argument.", intArg.ArgumentString(0, intArg.ArgumentNameLength(true), true));
    PrimitiveArgument strArg = RpcArgument::OptionalHexString("strArg", "\"\"", "This is a string argument.");
    EXPECT_EQ("\"strArg\"  (string, optional, default=\"\") This is a string argument.", strArg.ArgumentString(0, strArg.ArgumentNameLength(true), true));
}

TEST(DocBuilderTests, NamedPrimitiveArgumentsAreShown) {
    PrimitiveArgument unnamedHexArg = RpcArgument::HexString("This is an unnamed hex string result.");
    EXPECT_EQ("\"hex\"  (string) This is an unnamed hex string result.", unnamedHexArg.ArgumentString(0, unnamedHexArg.ArgumentNameLength(false), false));
    PrimitiveArgument namedHexArg = RpcArgument::HexString("hexArg", "This is a named hex string result.");
    EXPECT_EQ("\"hexArg\"  (string) This is a named hex string result.", namedHexArg.ArgumentString(0, namedHexArg.ArgumentNameLength(false), false));
}

TEST(DocBuilderTests, PrimitiveAlignsWithObject) {
std::string testDoc = RpcDocBuilder("testrpc")
        .SetDescription("This is a description.")
        .AddArgument(RpcArgument::String("stringArg", "This is a string argument."))
        .AddArgument(RpcArgument::Array("arrayArg", "This is an array argument.")
            .Of(RpcArgument::Object()
                .With(RpcArgument::String("objStrArg", "This is a string in an object."))
                .With(RpcArgument::Integer("objIntArg", "This is an int in an object."))))
        .AddResult(RpcArgument::String("This is a result."))
        .ToString();
    std::string expectedDoc = "testrpc \"stringArg\" [{\"objStrArg\": \"xxx\", \"objIntArg\": n}, ...]\n"
        "\n"
        "This is a description.\n"
        "\n"
        "Arguments:\n"
        "1. \"stringArg\"                (string, required) This is a string argument.\n"
        "2. arrayArg                   (json array of json objects, required) This is an array argument.\n"
        "     [\n"
        "       {                      (json object, required)\n"
        "         \"objStrArg\": \"xxx\",  (string, required) This is a string in an object.\n"
        "         \"objIntArg\": n       (numeric, required) This is an int in an object.\n"
        "       },\n"
        "       ...\n"
        "     ]\n"
        "\n"
        "Result:\n"
        "\"xxx\"  (string) This is a result.\n";
    EXPECT_EQ(expectedDoc, testDoc);
}

#![deny(unsafe_code)]
const INVENTORY_TEMPLATE_PREFIX: &str = "
all:
  hosts:
    zcash-ci-worker-unix:
      ansible_host: ";
const INVENTORY_TEMPLATE_SUFFIX: &str = "
      ansible_ssh_user: ubuntu";

const BBCONFIG_TEMPLATE_PREFIX: &str = "
---
buildbot_worker_user: zcbbworker
buildbot_master_host: ";
const BBCONFIG_TEMPLATE_INFIX: &str = "
buildbot_master_port: ";
const BBCONFIG_TEMPLATE_SUFFIX: &str = "
buildbot_worker_host_template: templates/host.j2";

const RUSOTO_PROVISION_VARS_TEMPLATE_PREFIX: &str = "
---
buildbot_worker_admin: ";
const RUSOTO_PROVISION_VARS_TEMPLATE_INFIX: &str = "
buildbot_worker_name: ";
const RUSOTO_PROVISION_VARS_TEMPLATE_SUFFIX: &str = "
buildbot_worker_password: ";
use rusoto_ec2::{
    DescribeInstancesRequest, DescribeInstancesResult, Ec2, Filter, Reservation, Tag,
    TagSpecification,
};
fn main() {
    let current_dir = std::env::current_dir().expect("Couldn't create current dir PathBuf");
    let parent_dir = current_dir
        .parent()
        .expect("Couldn't create parent dir PathBuf!");
    println!("{:#?}", parent_dir);
    std::env::set_current_dir(parent_dir);
    let bb_yaml = format!(
        "{}{}{}{}{}",
        BBCONFIG_TEMPLATE_PREFIX,
        std::env::var("BBMASTER_HOSTNAME").expect("Unable to determine bbmaster hostname!"),
        BBCONFIG_TEMPLATE_INFIX,
        std::env::var("BBMASTER_PORT").expect("Unable to determine bbmaster port!"),
        BBCONFIG_TEMPLATE_SUFFIX
    );
    std::fs::write("vars/buildbot.yml", bb_yaml);
    let rusoto_provision_yaml = format!(
        "{}{}{}{}{}{}",
        RUSOTO_PROVISION_VARS_TEMPLATE_PREFIX,
        std::env::var("BB_WORKER_ADMIN").expect("Unable to determine bb worker admin!"),
        RUSOTO_PROVISION_VARS_TEMPLATE_INFIX,
        std::env::var("BB_WORKER_NAME").expect("Unable to determine bb worker name!"),
        RUSOTO_PROVISION_VARS_TEMPLATE_SUFFIX,
        std::env::var("BB_WORKER_PASS").expect("Unable to determine bb worker passwd!"),
    );
    std::fs::write("vars/rusoto_provision.yml", rusoto_provision_yaml);
    let client = rusoto_ec2::Ec2Client::new(rusoto_core::Region::UsEast2);
    let key_pathname = &std::env::var("PRIVATE_SSH_KEY").unwrap();
    let key_filepath = *key_pathname
        .split("/")
        .collect::<Vec<&str>>()
        .last()
        .unwrap();
    let run_instance_request = rusoto_ec2::RunInstancesRequest {
        dry_run: Some(false),
        image_id: Some(String::from("ami-0b3c43897b5d26f4a")),
        min_count: 1,
        max_count: 1,
        key_name: Some(String::from(key_filepath)),
        instance_type: Some(String::from("m5.4xlarge")),
        tag_specifications: Some(vec![TagSpecification {
            resource_type: Some(String::from("instance")),
            tags: Some(vec![Tag {
                key: Some(String::from("amibuilder")),
                value: Some(String::from("test1")),
            }]),
        }]),
        ..Default::default()
    };
    client
        .run_instances(run_instance_request)
        .sync()
        .unwrap()
        .instances
        .unwrap();
    let describe_instance_request = DescribeInstancesRequest {
        dry_run: Some(false),
        filters: Some(vec![
            Filter {
                name: Some(String::from("tag:amibuilder")),
                values: Some(vec![String::from("test1")]),
            },
            Filter {
                name: Some(String::from("instance-state-code")),
                values: Some(vec![String::from("16")]),
            },
        ]),
        ..Default::default()
    };
    let reservations = loop {
        let reservations = extract_reservations(
            client
                .describe_instances(describe_instance_request.clone())
                .sync()
                .unwrap(),
        );
        if *reservations != [] {
            break reservations;
        }
        std::thread::sleep(std::time::Duration::new(1, 500_000_000));
    };
    let pub_ip = loop {
        let pub_ip = extract_pub_ip(&extract_reservations(
            client
                .describe_instances(describe_instance_request.clone())
                .sync()
                .unwrap(),
        ));
        if pub_ip != "" {
            break pub_ip;
        }
        std::thread::sleep(std::time::Duration::new(1, 500_000_000));
    };
    let hosts_text = format!(
        "{}{}{}",
        INVENTORY_TEMPLATE_PREFIX,
        &pub_ip.replace("\"", ""),
        INVENTORY_TEMPLATE_SUFFIX
    );
    std::fs::write("inventory/hosts", hosts_text).expect("Write failure!");
    let ssh_out = loop {
        let output = std::process::Command::new("ssh")
            .args(&["-o", "StrictHostKeyChecking=no"])
            .args(&["-i", key_pathname])
            .arg(format!("ubuntu@{}", pub_ip))
            .output()
            .expect("Couldn't run ssh");
        let mut err_output = String::from_utf8(output.stderr).unwrap();
        let error_end = err_output.split_off(92);
        if err_output != "Pseudo-terminal will not be allocated because stdin is not a terminal.\r\nssh: connect to host" {
            break output.stdout;
        }
        std::thread::sleep(std::time::Duration::new(10, 0));
    };
    println!("About to make blocking call to ansible-playbook!");
    std::process::Command::new("ansible-playbook")
        .args(&["-e", "buildbot_worker_host_template=templates/host.ec2.j2"])
        .arg(format!("--private-key={}", key_pathname))
        .args(&["-i", "inventory/hosts"])
        .arg("unix.yml")
        .output()
        .expect("ansible-playbook invocation failed!");
    println!("ansible-playbook completed without an error being detected in the process return!");
}

fn extract_reservations(describe_instances_result: DescribeInstancesResult) -> Vec<Reservation> {
    describe_instances_result.reservations.unwrap()
}

fn extract_pub_ip(reservations: &Vec<Reservation>) -> String {
    assert_eq!(reservations.len(), 1);
    reservations[0].instances.as_ref().unwrap()[0]
        .public_ip_address
        .as_ref()
        .unwrap()
        .clone()
}
